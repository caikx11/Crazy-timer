#pragma once

#include "schedule_validation.hpp"
#include <map>
#include <algorithm>

namespace power_timer {

// The host serializes commands, resume notifications and timer delivery.
// This module emits intentions only and never calls Windows power APIs.
class ScheduleBook {
public:
    explicit ScheduleBook(ScheduleStorage& storage) : storage_(storage) {}
    ScheduleBook(ScheduleStorage& storage, Calendar& calendar) : storage_(storage), calendar_(&calendar) {}

    bool initialize(ClockSample now) {
        ready_ = false;
        snapshot_ = {};
        deadlines_.clear();
        const auto loaded = storage_.load();
        if (loaded.status == LoadStatus::Unavailable) return fail(BookError::Storage);
        if (loaded.status != LoadStatus::Missing &&
            (loaded.status != LoadStatus::Loaded || !valid_snapshot(loaded.snapshot)))
            return fail(BookError::InvalidData);
        for (const auto& record : loaded.snapshot.records) {
            if (record.kind == ScheduleKind::Duration && record.state == OccurrenceState::Scheduled && record.due_utc_ms > now.utc_ms &&
                record.due_utc_ms - now.utc_ms > maximum - now.monotonic_ms)
                return fail(BookError::InvalidData);
        }
        snapshot_ = loaded.status == LoadStatus::Missing ? ScheduleSnapshot{} : loaded.snapshot;
        for (const auto& record : snapshot_.records) {
            if (record.kind == ScheduleKind::Duration && record.state == OccurrenceState::Scheduled)
                deadlines_[record.id] = now.monotonic_ms +
                    (record.due_utc_ms > now.utc_ms ? record.due_utc_ms - now.utc_ms : 0);
        }
        ready_ = true;
        return reconcile(now);
    }

    bool reconcile(ClockSample now) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        bool changed = false;
        for (auto& record : candidate.records) {
            if (!calendar_available(record, now)) { ready_ = false; return false; }
            if (record.kind == ScheduleKind::Weekly) {
                const auto before = record;
                if (!refresh_weekly(candidate, record, now)) { ready_ = false; return false; }
                changed = changed || before.due_utc_ms != record.due_utc_ms ||
                    before.occurrence_id != record.occurrence_id || before.civil_high_water != record.civil_high_water;
                continue;
            }
            if (record.state != OccurrenceState::Scheduled) continue;
            const auto remaining = remaining_ms(record.id, now);
            if (remaining == 0) {
                record.state = OccurrenceState::Expired;
                changed = true;
            } else if (record.kind == ScheduleKind::Duration) {
                // Checkpoint civil-clock changes while retaining the session's
                // elapsed-time deadline. A restart restores this durable anchor.
                if (now.utc_ms > maximum - remaining) {
                    ready_ = false;
                    return fail(BookError::InvalidData);
                }
                const auto due = now.utc_ms + remaining;
                if (record.due_utc_ms != due) {
                    record.due_utc_ms = due;
                    changed = true;
                }
            }
        }
        if (!update_conflicts(candidate, now, changed)) { ready_ = false; return false; }
        if (changed && !commit(candidate)) { ready_ = false; return false; }
        error_ = BookError::None;
        return true;
    }

    std::vector<Delivery> tick(ClockSample now) {
        if (!ready_) return {};
        auto candidate = snapshot_;
        std::vector<Delivery> deliveries;
        bool changed = false;
        std::unordered_set<std::uint64_t> replanned;
        // Refresh every rule first: a later row may move onto an earlier row's
        // deadline. No intent may be produced until the complete set is checked.
        for (auto& record : candidate.records) {
            if (!calendar_available(record, now)) { ready_ = false; return {}; }
            if (record.kind != ScheduleKind::Weekly) continue;
            const auto values = calendar_->resolve(record.civil);
            if (!values) { fail(BookError::Calendar); ready_ = false; return {}; }
            if (values->empty() || values->front() != record.due_utc_ms) {
                if (values->empty()) {
                    if (!refresh_weekly(candidate, record, now)) { ready_ = false; return {}; }
                } else record.due_utc_ms = values->front();
                changed = true;
                replanned.insert(record.id);
            }
        }
        if (!update_conflicts(candidate, now, changed)) { ready_ = false; return {}; }
        for (auto& record : candidate.records) {
            if (record.conflict_with) continue;
            if (replanned.contains(record.id)) {
                if (!refresh_weekly(candidate, record, now)) { ready_ = false; return {}; }
                continue;
            }
            if (record.state != OccurrenceState::Scheduled || remaining_ms(record.id, now) != 0) continue;
            changed = true;
            const auto late = record.kind == ScheduleKind::Duration
                ? now.monotonic_ms - deadlines_.at(record.id) : now.utc_ms - record.due_utc_ms;
            const bool deliver = !record.paused && late <= 5000;
            record.state = deliver ? OccurrenceState::Uncertain : OccurrenceState::Expired;
            if (deliver) deliveries.push_back({record.id, record.occurrence_id, record.action});
            if (record.kind == ScheduleKind::Weekly) {
                consume_weekly(record, record.state);
                if (!advance_weekly(candidate, record, now)) { ready_ = false; return {}; }
            }
        }
        if (changed && !commit(candidate)) { ready_ = false; return {}; }
        return deliveries;
    }

    bool complete(Delivery delivery) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto record = find(candidate, delivery.id);
        if (record == candidate.records.end()) return fail(BookError::NotFound);
        const bool weekly = record->kind == ScheduleKind::Weekly;
        const auto occurrence_id = weekly ? record->last_occurrence_id : record->occurrence_id;
        auto& result = weekly ? record->last_state : record->state;
        if (occurrence_id != delivery.occurrence_id || record->action != delivery.action ||
            (result != OccurrenceState::Uncertain && result != OccurrenceState::Completed))
            return fail(BookError::InvalidInput);
        if (result == OccurrenceState::Completed) { error_ = BookError::None; return true; }
        result = OccurrenceState::Completed;
        if (!commit(candidate)) { ready_ = false; return false; }
        return true;
    }

    bool create(Action action, std::wstring_view text, ClockSample now) {
        if (!ready_) return false;
        const auto minutes = Scheduler::parse_duration(text);
        if (!minutes || !valid_action(action) || !valid_deadline(now, *minutes) ||
            snapshot_.records.size() >= max_schedule_records || snapshot_.next_id == maximum ||
            snapshot_.next_occurrence_id == maximum) return fail(BookError::InvalidInput);
        auto candidate = snapshot_;
        const auto id = candidate.next_id++;
        candidate.records.push_back({id, candidate.next_occurrence_id++, action,
            *minutes, now.utc_ms + *minutes * 60000, false, OccurrenceState::Scheduled});
        if (!accept_candidate(candidate, id, now) || !commit(candidate)) return false;
        deadlines_[id] = now.monotonic_ms + *minutes * 60000;
        return true;
    }

    bool edit(std::uint64_t id, Action action, std::wstring_view text, ClockSample now) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto record = find(candidate, id);
        if (record == candidate.records.end()) return fail(BookError::NotFound);
        const auto minutes = Scheduler::parse_duration(text);
        if (!minutes || !valid_action(action) || !valid_deadline(now, *minutes) ||
            snapshot_.next_occurrence_id == maximum) return fail(BookError::InvalidInput);
        const bool paused = record->paused;
        const bool force = record->force_shutdown && action == Action::Shutdown;
        *record = {id, 0, action, 0, 0, paused, OccurrenceState::Scheduled};
        record->force_shutdown = force;
        record->action = action;
        record->minutes = *minutes;
        record->due_utc_ms = now.utc_ms + *minutes * 60000;
        record->occurrence_id = candidate.next_occurrence_id++;
        record->state = OccurrenceState::Scheduled;
        if (!accept_candidate(candidate, id, now) || !commit(candidate)) return false;
        deadlines_[id] = now.monotonic_ms + *minutes * 60000;
        return true;
    }

    std::optional<std::uint64_t> preview_once(CivilTime civil, ClockSample now) const {
        BookError error;
        return resolve_once(civil, now, error);
    }
    bool create_once(Action action, CivilTime civil, ClockSample now) {
        return save_once(0, action, civil, now);
    }
    bool edit_once(std::uint64_t id, Action action, CivilTime civil, ClockSample now) {
        if (id == 0) return fail(BookError::NotFound);
        return save_once(id, action, civil, now);
    }
    std::optional<std::uint64_t> preview_weekly(unsigned weekdays, unsigned minute_of_day, ClockSample now) const {
        ScheduleRecord record{};
        record.weekdays = weekdays;
        record.minute_of_day = minute_of_day;
        BookError error;
        if (!plan_weekly(record, now, error)) return {};
        return record.due_utc_ms;
    }
    bool create_weekly(Action action, unsigned weekdays, unsigned minute_of_day, ClockSample now) {
        return save_weekly(0, action, weekdays, minute_of_day, now);
    }
    bool edit_weekly(std::uint64_t id, Action action, unsigned weekdays, unsigned minute_of_day, ClockSample now) {
        if (id == 0) return fail(BookError::NotFound);
        return save_weekly(id, action, weekdays, minute_of_day, now);
    }

    bool set_paused(std::uint64_t id, bool paused, ClockSample now) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto record = find(candidate, id);
        if (record == candidate.records.end()) return fail(BookError::NotFound);
        if (record->kind == ScheduleKind::Weekly) {
            if (!refresh_weekly(candidate, *record, now)) return false;
        } else if (record->state == OccurrenceState::Scheduled && remaining_ms(id, now) == 0)
            record->state = OccurrenceState::Expired;
        else if (record->paused == paused) { error_ = BookError::None; return true; }
        record->paused = paused;
        return accept_candidate(candidate, id, now, false) && commit(candidate);
    }

    bool erase(std::uint64_t id) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto record = find(candidate, id);
        if (record == candidate.records.end()) return fail(BookError::NotFound);
        candidate.records.erase(record);
        bool changed = false;
        if (!update_conflicts(candidate, last_now_, changed)) return false;
        if (!commit(candidate)) return false;
        deadlines_.erase(id);
        return true;
    }

    bool cancel(std::uint64_t id) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto record = find(candidate, id);
        if (record == candidate.records.end()) return fail(BookError::NotFound);
        if (record->state != OccurrenceState::Scheduled) { error_ = BookError::None; return true; }
        record->state = OccurrenceState::Cancelled;
        if (record->kind == ScheduleKind::Weekly) {
            consume_weekly(*record, OccurrenceState::Cancelled);
            if (!advance_weekly(candidate, *record, {record->due_utc_ms, 0})) return false;
        }
        bool changed = false;
        if (!update_conflicts(candidate, last_now_, changed)) return false;
        return commit(candidate);
    }

    const std::vector<ScheduleRecord>& list() const { return snapshot_.records; }
    std::uint64_t remaining_ms(std::uint64_t id, ClockSample now) const {
        const auto record = std::find_if(snapshot_.records.begin(), snapshot_.records.end(),
            [id](const auto& value) { return value.id == id; });
        if (record == snapshot_.records.end() || record->state != OccurrenceState::Scheduled) return 0;
        if (record->kind != ScheduleKind::Duration)
            return record->due_utc_ms > now.utc_ms ? record->due_utc_ms - now.utc_ms : 0;
        const auto deadline = deadlines_.find(id);
        return deadline != deadlines_.end() && deadline->second > now.monotonic_ms
            ? deadline->second - now.monotonic_ms : 0;
    }
    bool writable() const { return ready_; }
    BookError error() const { return error_; }
    std::uint64_t conflicting_id() const { return conflicting_id_; }

private:
    static constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    static constexpr std::uint64_t calendar_end = 253402300800000ULL;
    static constexpr std::uint64_t invalid_due = std::numeric_limits<std::uint64_t>::max();
    bool calendar_available(const ScheduleRecord& record, ClockSample now) {
        if (record.kind == ScheduleKind::Duration) return true;
        if (now.utc_ms >= calendar_end) return fail(BookError::InvalidData);
        if (!calendar_) return fail(BookError::Calendar);
        const auto local = calendar_->local(now.utc_ms);
        return local && valid_civil(*local) ? true : fail(BookError::Calendar);
    }
    // A civil high-water mark is durable even for dates skipped by a gap or clock change.
    bool plan_weekly(ScheduleRecord& record, ClockSample now, BookError& error) const {
        error = BookError::InvalidInput;
        if (!record.weekdays || record.weekdays > 127 || record.minute_of_day >= 1440 || now.utc_ms >= calendar_end)
            return false;
        if (!calendar_) { error = BookError::Calendar; return false; }
        const auto local = calendar_->local(now.utc_ms);
        if (!local || !valid_civil(*local)) { error = BookError::Calendar; return false; }
        record.civil_high_water = std::max(record.civil_high_water, civil_key(*local));
        auto day = *local;
        if (const auto high = civil_from_key(record.civil_high_water); high && civil_key(*high) > civil_key(day)) day = *high;
        day.hour = record.minute_of_day / 60;
        day.minute = record.minute_of_day % 60;
        // Ordinary weekly rules resolve within eight days. A whole year without
        // a reachable date indicates an unusable adapter; fail explicitly rather
        // than scanning thousands of years on the UI thread or dropping the rule.
        for (unsigned searched_days = 0; searched_days < 366; ++searched_days) {
            const auto key = civil_key(day);
            if (key > record.civil_high_water && (record.weekdays & (1U << weekday(day)))) {
                const auto values = calendar_->resolve(day);
                if (!values) { error = BookError::Calendar; return false; }
                if (!values->empty() && values->front() >= calendar_end) return false;
                if (!values->empty() && values->front() > now.utc_ms) {
                    record.civil = day;
                    record.due_utc_ms = values->front();
                    error = BookError::None;
                    return true;
                }
                record.civil_high_water = key;
            }
            const auto next = next_day(day);
            if (!next) return false;
            day = *next;
        }
        error = BookError::Calendar;
        return false;
    }
    static void consume_weekly(ScheduleRecord& record, OccurrenceState result) {
        record.last_occurrence_id = record.occurrence_id;
        record.last_state = result;
        record.civil_high_water = std::max(record.civil_high_water, civil_key(record.civil));
        record.state = result;
    }
    bool advance_weekly(ScheduleSnapshot& candidate, ScheduleRecord& record, ClockSample now) {
        BookError error;
        if (!plan_weekly(record, now, error)) return fail(error);
        if (candidate.next_occurrence_id == maximum) return fail(BookError::InvalidInput);
        record.occurrence_id = candidate.next_occurrence_id++;
        record.state = OccurrenceState::Scheduled;
        return true;
    }
    bool refresh_weekly(ScheduleSnapshot& candidate, ScheduleRecord& record, ClockSample now) {
        if (!calendar_) return fail(BookError::Calendar);
        const auto values = calendar_->resolve(record.civil);
        if (!values) return fail(BookError::Calendar);
        if (record.state == OccurrenceState::Scheduled &&
            (record.due_utc_ms <= now.utc_ms || values->empty() || values->front() <= now.utc_ms))
            consume_weekly(record, OccurrenceState::Expired);
        if (record.state != OccurrenceState::Scheduled) return advance_weekly(candidate, record, now);
        const auto previous_civil = record.civil;
        BookError error;
        if (!plan_weekly(record, now, error)) return fail(error);
        if (!(previous_civil == record.civil)) {
            if (candidate.next_occurrence_id == maximum) return fail(BookError::InvalidInput);
            record.occurrence_id = candidate.next_occurrence_id++;
        }
        return true;
    }
    bool save_weekly(std::uint64_t id, Action action, unsigned weekdays, unsigned minute_of_day, ClockSample now) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        const auto old = find(candidate, id);
        if (id && old == candidate.records.end()) return fail(BookError::NotFound);
        if (!valid_action(action) || candidate.next_occurrence_id == maximum ||
            (!id && (candidate.records.size() >= max_schedule_records || candidate.next_id == maximum)))
            return fail(BookError::InvalidInput);
        ScheduleRecord record{id ? id : candidate.next_id++, candidate.next_occurrence_id++, action,
            0, 0, id ? old->paused : false, OccurrenceState::Scheduled};
        record.force_shutdown = id && old->force_shutdown && action == Action::Shutdown;
        record.kind = ScheduleKind::Weekly;
        record.weekdays = weekdays;
        record.minute_of_day = minute_of_day;
        BookError error;
        if (!plan_weekly(record, now, error)) return fail(error);
        if (id) *old = record; else candidate.records.push_back(record);
        if (!accept_candidate(candidate, record.id, now) || !commit(candidate)) return false;
        deadlines_.erase(record.id);
        return true;
    }
    std::optional<std::uint64_t> resolve_once(CivilTime civil, ClockSample now, BookError& error) const {
        error = BookError::InvalidInput;
        if (!valid_civil(civil) || now.utc_ms >= calendar_end) return {};
        if (!calendar_) { error = BookError::Calendar; return {}; }
        const auto values = calendar_->resolve(civil);
        if (!values) { error = BookError::Calendar; return {}; }
        if (values->empty() || values->front() <= now.utc_ms || values->front() >= calendar_end) return {};
        error = BookError::None;
        return values->front();
    }
    bool save_once(std::uint64_t id, Action action, CivilTime civil, ClockSample now) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto old = find(candidate, id);
        if (id && old == candidate.records.end()) return fail(BookError::NotFound);
        BookError error;
        const auto due = resolve_once(civil, now, error);
        if (!due) return fail(error);
        if (!valid_action(action) || candidate.next_occurrence_id == maximum ||
            (!id && (candidate.records.size() >= max_schedule_records || candidate.next_id == maximum)))
            return fail(BookError::InvalidInput);
        ScheduleRecord record{id ? id : candidate.next_id++, candidate.next_occurrence_id++, action,
            0, *due, id ? old->paused : false, OccurrenceState::Scheduled};
        record.force_shutdown = id && old->force_shutdown && action == Action::Shutdown;
        record.kind = ScheduleKind::Once;
        record.civil = civil;
        if (id) *old = record; else candidate.records.push_back(record);
        if (!accept_candidate(candidate, record.id, now) || !commit(candidate)) return false;
        deadlines_.erase(record.id);
        return true;
    }
    struct ConflictTime {
        std::uint64_t due = 0, civil = 0;
        unsigned weekday_bit = 0, minute = 0;
    };
    bool update_conflicts(ScheduleSnapshot& candidate, ClockSample now, bool& changed,
                          std::uint64_t replaced_id = 0) {
        last_now_ = now;
        const bool has_weekly = std::any_of(candidate.records.begin(), candidate.records.end(),
            [](const auto& row) { return !row.paused && row.kind == ScheduleKind::Weekly; });
        std::vector<ConflictTime> times(candidate.records.size());
        std::vector<std::uint64_t> conflicts(candidate.records.size());
        for (std::size_t i = 0; i < candidate.records.size(); ++i) {
            const auto& row = candidate.records[i];
            if (row.paused || row.state != OccurrenceState::Scheduled) continue;
            auto& time = times[i];
            time.due = row.due_utc_ms;
            const auto deadline = deadlines_.find(row.id);
            if (row.kind == ScheduleKind::Duration && row.id != replaced_id && deadline != deadlines_.end()) {
                if (deadline->second >= now.monotonic_ms) {
                    const auto remaining = deadline->second - now.monotonic_ms;
                    if (now.utc_ms > maximum - remaining) return fail(BookError::InvalidData);
                    time.due = now.utc_ms + remaining;
                } else {
                    const auto late = now.monotonic_ms - deadline->second;
                    // An elapsed deadline before the UTC epoch cannot match a civil rule.
                    if (late > now.utc_ms) {
                        time.due = row.due_utc_ms;
                        // civil fields remain zero/unset
                    } else {
                        time.due = now.utc_ms - late;
                    }
                }
                // Update the record's due_utc_ms to the projected civil time for persistence across restarts
                if (row.due_utc_ms != time.due) {
                    candidate.records[i].due_utc_ms = time.due;
                    changed = true;
                }
            }
            if (!has_weekly || row.kind == ScheduleKind::Weekly || time.due >= calendar_end) continue;
            if (!calendar_) return fail(BookError::Calendar);
            const auto local = calendar_->local(time.due);
            if (!local || !valid_civil(*local)) return fail(BookError::Calendar);
            const auto values = calendar_->resolve(*local);
            if (!values) return fail(BookError::Calendar);
            // Only an exact match to the first instant of a civil minute can be
            // a weekly occurrence; second-fold instants and milliseconds differ.
            if (!values->empty() && values->front() == time.due) {
                time.civil = civil_key(*local);
                time.weekday_bit = 1U << weekday(*local);
                time.minute = local->hour * 60 + local->minute;
            }
        }
        for (std::size_t i = 0; i < candidate.records.size(); ++i) {
            const auto& a = candidate.records[i];
            if (a.paused || a.state != OccurrenceState::Scheduled) continue;
            for (std::size_t j = i + 1; j < candidate.records.size(); ++j) {
                const auto& b = candidate.records[j];
                if (b.paused || b.state != OccurrenceState::Scheduled) continue;
                bool collision;
                if (a.kind == ScheduleKind::Weekly && b.kind == ScheduleKind::Weekly) {
                    collision = (a.weekdays & b.weekdays) && a.minute_of_day == b.minute_of_day;
                } else if (a.kind == ScheduleKind::Weekly || b.kind == ScheduleKind::Weekly) {
                    const auto& weekly = a.kind == ScheduleKind::Weekly ? a : b;
                    const auto& finite = a.kind == ScheduleKind::Weekly ? times[j] : times[i];
                    collision = finite.civil > weekly.civil_high_water &&
                        (finite.weekday_bit & weekly.weekdays) && finite.minute == weekly.minute_of_day;
                } else collision = times[i].due == times[j].due;
                if (!collision) continue;
                // A deterministic neighbour identifies the component without
                // depending on row order or silently selecting an action.
                if (!conflicts[i] || b.id < conflicts[i]) conflicts[i] = b.id;
                if (!conflicts[j] || a.id < conflicts[j]) conflicts[j] = a.id;
            }
        }
        for (std::size_t i = 0; i < candidate.records.size(); ++i) {
            changed = changed || candidate.records[i].conflict_with != conflicts[i];
            candidate.records[i].conflict_with = conflicts[i];
        }
        return true;
    }
    bool accept_candidate(ScheduleSnapshot& candidate, std::uint64_t id, ClockSample now,
                          bool replaced = true) {
        bool changed = false;
        if (!update_conflicts(candidate, now, changed, replaced ? id : 0)) return false;
        const auto selected = find(candidate, id);
        if (selected != candidate.records.end() && selected->conflict_with) {
            conflicting_id_ = selected->conflict_with;
            return fail(BookError::Conflict);
        }
        return true;
    }
    static bool valid_deadline(ClockSample now, std::uint64_t minutes) {
        return now.utc_ms <= maximum - minutes * 60000 &&
               now.monotonic_ms <= maximum - minutes * 60000;
    }
    static std::vector<ScheduleRecord>::iterator find(ScheduleSnapshot& snapshot, std::uint64_t id) {
        return std::find_if(snapshot.records.begin(), snapshot.records.end(),
            [id](const auto& record) { return record.id == id; });
    }
    bool fail(BookError error) { error_ = error; if (error != BookError::Conflict) conflicting_id_ = 0; return false; }
    bool commit(const ScheduleSnapshot& candidate) {
        if (!storage_.save(candidate)) return fail(BookError::Storage);
        snapshot_ = candidate;
        conflicting_id_ = 0;
        error_ = BookError::None;
        return true;
    }

public:
    // Getter for theme preference
    unsigned get_theme() const {
        return snapshot_.theme;
    }

    // Setter for theme preference
    bool set_theme(unsigned theme) {
        if (theme > 2) return false;
        auto candidate = snapshot_;
        candidate.theme = theme;
        return commit(candidate);
    }

    // Getter for sound enabled
    bool get_sound_enabled() const {
        return snapshot_.sound_enabled;
    }
    ScheduleStorage& storage_;
    Calendar* calendar_ = nullptr;
    ScheduleSnapshot snapshot_;
    std::map<std::uint64_t, std::uint64_t> deadlines_;
    bool ready_ = false;
    BookError error_ = BookError::NotReady;
    std::uint64_t conflicting_id_ = 0;
    ClockSample last_now_{};
};

} // namespace power_timer


