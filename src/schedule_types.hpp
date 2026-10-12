#pragma once

#include "scheduler.hpp"
#include "calendar.hpp"
#include <vector>

namespace power_timer {

struct ClockSample { std::uint64_t utc_ms, monotonic_ms; };
enum class OccurrenceState { Scheduled, Cancelled, Expired, Uncertain, Completed };
enum class ScheduleKind { Duration, Once, Weekly };
struct ScheduleRecord {
    std::uint64_t id, occurrence_id;
    Action action;
    std::uint64_t minutes, due_utc_ms;
    bool paused;
    OccurrenceState state;
    ScheduleKind kind = ScheduleKind::Duration;
    CivilTime civil{};
    unsigned weekdays = 0, minute_of_day = 0;
    std::uint64_t civil_high_water = 0;
    std::uint64_t last_occurrence_id = 0;
    OccurrenceState last_state = OccurrenceState::Scheduled;
    std::uint64_t conflict_with = 0;
    bool force_shutdown = false;
    // Weekly only: one-off postponement of the current occurrence; the rule's
    // usual time (civil/minute_of_day) is never changed. 0 means none.
    std::uint64_t postponed_utc_ms = 0;
};
// The instant at which the current occurrence is actually due.
inline std::uint64_t effective_due_utc_ms(const ScheduleRecord& record) {
    return record.postponed_utc_ms > record.due_utc_ms ? record.postponed_utc_ms : record.due_utc_ms;
}
struct ScheduleSnapshot {
    std::uint64_t next_id = 1, next_occurrence_id = 1;
    std::vector<ScheduleRecord> records;
    bool sound_enabled = false;
    unsigned theme = 0; // 0=automatic, 1=light, 2=dark
    unsigned locale = 0; // 0=pt-BR, 1=English
};
enum class LoadStatus { Missing, Loaded, Invalid, Unavailable };
struct LoadResult { LoadStatus status; ScheduleSnapshot snapshot; };
class ScheduleStorage {
public:
    virtual ~ScheduleStorage() = default;
    virtual LoadResult load() = 0;
    virtual bool save(const ScheduleSnapshot&) = 0;
};
struct Delivery { std::uint64_t id, occurrence_id; Action action; bool force = false; };
enum class BookError { None, InvalidInput, NotFound, Storage, InvalidData, NotReady, Calendar, Conflict };

} // namespace power_timer
