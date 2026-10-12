#pragma once

#include "scheduler.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <unordered_set>

namespace power_timer {

// Decides which countdown warning (if any) is due for an occurrence. Thresholds
// already crossed the first time an occurrence is seen were never "reached" by
// the countdown and are marked without notifying.
class WarningTracker {
public:
    static constexpr std::array<std::uint64_t, 5> thresholds = {
        30ULL * 60000, 15ULL * 60000, 10ULL * 60000, 5ULL * 60000, Scheduler::warning_ms};

    // Returns the threshold index to announce now, or nothing.
    std::optional<std::size_t> due(std::uint64_t occurrence_id, std::uint64_t remaining_ms) {
        if (remaining_ms == 0) return std::nullopt;
        const bool first_sight = keys_.insert(occurrence_id * 10 + 9).second;
        int applicable = -1;
        for (int i = 0; i < static_cast<int>(thresholds.size()); ++i)
            if (remaining_ms <= thresholds[i]) applicable = i;
        if (applicable < 0) return std::nullopt;
        for (int i = 0; i < applicable; ++i) keys_.insert(occurrence_id * 10 + i);
        const bool fresh = keys_.insert(occurrence_id * 10 + applicable).second;
        if (first_sight || !fresh) return std::nullopt;
        return static_cast<std::size_t>(applicable);
    }
    // After a postponement only the warnings still ahead of the new deadline may fire again.
    void reopen_ahead_of(std::uint64_t occurrence_id, std::uint64_t remaining_ms) {
        for (std::size_t i = 0; i < thresholds.size(); ++i)
            if (remaining_ms > thresholds[i]) keys_.erase(occurrence_id * 10 + i);
    }

private:
    std::unordered_set<std::uint64_t> keys_; // occurrence_id * 10 + index; +9 marks "seen"
};

} // namespace power_timer
