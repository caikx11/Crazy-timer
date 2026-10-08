#pragma once

#include "schedule_types.hpp"
#include <unordered_set>

namespace power_timer {

inline constexpr std::size_t max_schedule_records = 1000;

inline bool valid_action(Action action) {
    return action == Action::Shutdown || action == Action::Sleep || action == Action::Hibernate;
}

inline bool valid_snapshot(const ScheduleSnapshot& snapshot) {
    if (snapshot.next_id == 0 || snapshot.next_occurrence_id == 0 ||
        snapshot.records.size() > max_schedule_records) return false;
    std::unordered_set<std::uint64_t> ids, occurrences;
    for (const auto& record : snapshot.records) {
        if (record.id == 0 || record.id >= snapshot.next_id || record.occurrence_id == 0 ||
            record.occurrence_id >= snapshot.next_occurrence_id || !ids.insert(record.id).second ||
            !occurrences.insert(record.occurrence_id).second || !valid_action(record.action) ||
            record.minutes < Scheduler::min_minutes || record.minutes > Scheduler::max_minutes ||
            (record.state != OccurrenceState::Scheduled && record.state != OccurrenceState::Cancelled &&
             record.state != OccurrenceState::Expired && record.state != OccurrenceState::Uncertain &&
             record.state != OccurrenceState::Completed)) return false;
    }
    return true;
}

} // namespace power_timer
