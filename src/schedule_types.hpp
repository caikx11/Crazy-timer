#pragma once

#include "scheduler.hpp"
#include <vector>

namespace power_timer {

struct ClockSample { std::uint64_t utc_ms, monotonic_ms; };
enum class OccurrenceState { Scheduled, Cancelled, Expired, Uncertain, Completed };
struct ScheduleRecord {
    std::uint64_t id, occurrence_id;
    Action action;
    std::uint64_t minutes, due_utc_ms;
    bool paused;
    OccurrenceState state;
};
struct ScheduleSnapshot {
    std::uint64_t next_id = 1, next_occurrence_id = 1;
    std::vector<ScheduleRecord> records;
};
enum class LoadStatus { Missing, Loaded, Invalid, Unavailable };
struct LoadResult { LoadStatus status; ScheduleSnapshot snapshot; };
class ScheduleStorage {
public:
    virtual ~ScheduleStorage() = default;
    virtual LoadResult load() = 0;
    virtual bool save(const ScheduleSnapshot&) = 0;
};
struct Delivery { std::uint64_t id, occurrence_id; Action action; };
enum class BookError { None, InvalidInput, NotFound, Storage, InvalidData, NotReady };

} // namespace power_timer
