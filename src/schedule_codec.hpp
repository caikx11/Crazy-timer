#pragma once
#include "schedule_validation.hpp"
#include <charconv>
#include <string>

namespace power_timer {
inline constexpr std::size_t max_schedule_file_bytes = 1024 * 1024;
// Version 1 is canonical ASCII: exact spaces/newlines, decimal unsigned integers,
// no leading zeroes, and no ignored fields/trailing bytes. Invalid input encodes as empty.
inline std::string encode_snapshot(const ScheduleSnapshot& snapshot) {
    if (!valid_snapshot(snapshot)) return {};
    std::string data = "POWER_TIMER 1\n" + std::to_string(snapshot.next_id) + " " +
        std::to_string(snapshot.next_occurrence_id) + " " + std::to_string(snapshot.records.size()) + "\n";
    for (const auto& record : snapshot.records) {
        data += std::to_string(record.id) + " " + std::to_string(record.occurrence_id) + " " +
            std::to_string(static_cast<unsigned>(record.action)) + " " + std::to_string(record.minutes) + " " +
            std::to_string(record.due_utc_ms) + " " + (record.paused ? "1" : "0") + " " +
            std::to_string(static_cast<unsigned>(record.state)) + "\n";
    }
    return data;
}

inline std::optional<ScheduleSnapshot> decode_snapshot(std::string_view data) {
    constexpr std::string_view magic = "POWER_TIMER 1\n";
    if (!data.starts_with(magic) || data.size() > max_schedule_file_bytes) return std::nullopt;
    data.remove_prefix(magic.size());
    auto number = [&data](char delimiter) -> std::optional<std::uint64_t> {
        const auto end = data.find(delimiter);
        if (end == std::string_view::npos || end == 0) return std::nullopt;
        const auto token = data.substr(0, end);
        if (token.size() > 1 && token.front() == '0') return std::nullopt;
        if (token.find_first_not_of("0123456789") != std::string_view::npos) return std::nullopt;
        std::uint64_t value = 0;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) return std::nullopt;
        data.remove_prefix(end + 1);
        return value;
    };
    const auto next_id = number(' '), next_occurrence = number(' '), count = number('\n');
    if (!next_id || !next_occurrence || !count || *count > max_schedule_records) return std::nullopt;
    ScheduleSnapshot snapshot{*next_id, *next_occurrence, {}};
    for (std::uint64_t i = 0; i < *count; ++i) {
        const auto id = number(' '), occurrence = number(' '), action = number(' '), minutes = number(' '),
            due = number(' '), paused = number(' '), state = number('\n');
        if (!id || !occurrence || !action || !minutes || !due || !paused || !state || *paused > 1 || *action > 2 || *state > 4) return std::nullopt;
        snapshot.records.push_back({*id, *occurrence, static_cast<Action>(*action), *minutes,
            *due, *paused == 1, static_cast<OccurrenceState>(*state)});
    }
    if (!data.empty() || !valid_snapshot(snapshot)) return std::nullopt;
    return snapshot;
}
} // namespace power_timer
