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
            if (record.state == OccurrenceState::Scheduled && record.due_utc_ms > now.utc_ms &&
                record.due_utc_ms - now.utc_ms > maximum - now.monotonic_ms)
                return fail(BookError::InvalidData);
        }
        snapshot_ = loaded.status == LoadStatus::Missing ? ScheduleSnapshot{} : loaded.snapshot;
        for (const auto& record : snapshot_.records) {
            if (record.state == OccurrenceState::Scheduled)
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
            if (record.state != OccurrenceState::Scheduled) continue;
            const auto remaining = remaining_ms(record.id, now);
            if (remaining == 0) {
                record.state = OccurrenceState::Expired;
                changed = true;
            } else {
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
        if (changed && !commit(candidate)) { ready_ = false; return false; }
        error_ = BookError::None;
        return true;
    }

    std::vector<Delivery> tick(ClockSample now) {
        if (!ready_) return {};
        auto candidate = snapshot_;
        std::vector<Delivery> deliveries;
        bool changed = false;
        for (auto& record : candidate.records) {
            if (record.state != OccurrenceState::Scheduled || remaining_ms(record.id, now) != 0)
                continue;
            changed = true;
            record.state = record.paused ? OccurrenceState::Expired : OccurrenceState::Uncertain;
            if (!record.paused) deliveries.push_back({record.id, record.occurrence_id, record.action});
        }
        if (changed && !commit(candidate)) { ready_ = false; return {}; }
        return deliveries;
    }

    bool complete(Delivery delivery) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto record = find(candidate, delivery.id);
        if (record == candidate.records.end()) return fail(BookError::NotFound);
        if (record->occurrence_id != delivery.occurrence_id || record->action != delivery.action ||
            (record->state != OccurrenceState::Uncertain && record->state != OccurrenceState::Completed))
            return fail(BookError::InvalidInput);
        if (record->state == OccurrenceState::Completed) { error_ = BookError::None; return true; }
        record->state = OccurrenceState::Completed;
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
        if (!commit(candidate)) return false;
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
        record->action = action;
        record->minutes = *minutes;
        record->due_utc_ms = now.utc_ms + *minutes * 60000;
        record->occurrence_id = candidate.next_occurrence_id++;
        record->state = OccurrenceState::Scheduled;
        if (!commit(candidate)) return false;
        deadlines_[id] = now.monotonic_ms + *minutes * 60000;
        return true;
    }

    bool set_paused(std::uint64_t id, bool paused, ClockSample now) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto record = find(candidate, id);
        if (record == candidate.records.end()) return fail(BookError::NotFound);
        if (record->state == OccurrenceState::Scheduled && remaining_ms(id, now) == 0)
            record->state = OccurrenceState::Expired;
        else if (record->paused == paused) { error_ = BookError::None; return true; }
        record->paused = paused;
        return commit(candidate);
    }

    bool erase(std::uint64_t id) {
        if (!ready_) return false;
        auto candidate = snapshot_;
        auto record = find(candidate, id);
        if (record == candidate.records.end()) return fail(BookError::NotFound);
        candidate.records.erase(record);
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
        return commit(candidate);
    }

    const std::vector<ScheduleRecord>& list() const { return snapshot_.records; }
    std::uint64_t remaining_ms(std::uint64_t id, ClockSample now) const {
        const auto record = std::find_if(snapshot_.records.begin(), snapshot_.records.end(),
            [id](const auto& value) { return value.id == id; });
        if (record == snapshot_.records.end() || record->state != OccurrenceState::Scheduled) return 0;
        const auto deadline = deadlines_.find(id);
        return deadline != deadlines_.end() && deadline->second > now.monotonic_ms
            ? deadline->second - now.monotonic_ms : 0;
    }
    bool writable() const { return ready_; }
    BookError error() const { return error_; }

private:
    static constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
    static bool valid_deadline(ClockSample now, std::uint64_t minutes) {
        return now.utc_ms <= maximum - minutes * 60000 &&
               now.monotonic_ms <= maximum - minutes * 60000;
    }
    static std::vector<ScheduleRecord>::iterator find(ScheduleSnapshot& snapshot, std::uint64_t id) {
        return std::find_if(snapshot.records.begin(), snapshot.records.end(),
            [id](const auto& record) { return record.id == id; });
    }
    bool fail(BookError error) { error_ = error; return false; }
    bool commit(const ScheduleSnapshot& candidate) {
        if (!storage_.save(candidate)) return fail(BookError::Storage);
        snapshot_ = candidate;
        error_ = BookError::None;
        return true;
    }
    ScheduleStorage& storage_;
    ScheduleSnapshot snapshot_;
    std::map<std::uint64_t, std::uint64_t> deadlines_;
    bool ready_ = false;
    BookError error_ = BookError::NotReady;
};

} // namespace power_timer


