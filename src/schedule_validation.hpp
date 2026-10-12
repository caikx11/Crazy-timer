#pragma once

#include "schedule_types.hpp"
#include <unordered_set>

namespace power_timer {

inline constexpr std::size_t max_schedule_records = 1000;

inline bool valid_action(Action action) {
    return action == Action::Shutdown || action == Action::Sleep || action == Action::Hibernate;
}
inline bool valid_occurrence_state(OccurrenceState state) {
    return state == OccurrenceState::Scheduled || state == OccurrenceState::Cancelled ||
        state == OccurrenceState::Expired || state == OccurrenceState::Uncertain || state == OccurrenceState::Completed;
}

inline bool valid_snapshot(const ScheduleSnapshot& snapshot) {
    if (snapshot.next_id == 0 || snapshot.next_occurrence_id == 0 ||
        snapshot.records.size() > max_schedule_records) return false;
    // sound_enabled is a bool, no validation needed beyond type safety
    std::unordered_set<std::uint64_t> ids, occurrences;
    for (const auto& record : snapshot.records) {
        if (record.id == 0 || record.id >= snapshot.next_id || record.occurrence_id == 0 ||
            record.occurrence_id >= snapshot.next_occurrence_id || !ids.insert(record.id).second ||
            !occurrences.insert(record.occurrence_id).second || !valid_action(record.action) ||
            !valid_occurrence_state(record.state) || !valid_occurrence_state(record.last_state) ||
            (record.force_shutdown && record.action != Action::Shutdown) ||
            record.conflict_with >= snapshot.next_id || record.conflict_with == record.id) return false;
        if (record.postponed_utc_ms &&
            (record.kind != ScheduleKind::Weekly || record.postponed_utc_ms >= 253402300800000ULL)) return false;
        if (record.kind == ScheduleKind::Duration) {
            if (record.minutes < Scheduler::min_minutes || record.minutes > Scheduler::max_minutes ||
                !(record.civil == CivilTime{})) return false;
        } else if (record.kind == ScheduleKind::Once || record.kind == ScheduleKind::Weekly) {
            if (record.minutes != 0 || !valid_civil(record.civil) || record.due_utc_ms >= 253402300800000ULL)
                return false;
        } else return false;
        if (record.kind == ScheduleKind::Weekly) {
            if (!record.weekdays || record.weekdays > 127 || record.minute_of_day >= 1440 ||
                record.civil.hour * 60 + record.civil.minute != record.minute_of_day ||
                !(record.weekdays & (1U << weekday(record.civil))) ||
                record.state != OccurrenceState::Scheduled ||
                !civil_from_key(record.civil_high_water) ||
                record.civil_high_water >= civil_key(record.civil)) return false;
            if (record.last_occurrence_id) {
                if (record.last_occurrence_id >= record.occurrence_id ||
                    !occurrences.insert(record.last_occurrence_id).second ||
                    record.last_state == OccurrenceState::Scheduled) return false;
            } else if (record.last_state != OccurrenceState::Scheduled) return false;
        } else if (record.weekdays || record.minute_of_day || record.civil_high_water ||
                   record.last_occurrence_id || record.last_state != OccurrenceState::Scheduled) return false;
    }
    return true;
}

} // namespace power_timer
