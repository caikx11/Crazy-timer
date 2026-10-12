#pragma once
#include "schedule_validation.hpp"
#include <charconv>
#include <limits>
#include <string>

namespace power_timer {
inline constexpr std::size_t max_schedule_file_bytes = 1024 * 1024;
// Canonical ASCII: exact spaces/newlines, decimal unsigned integers, no leading
// zeroes, and no ignored fields/trailing bytes. Versions 1, 2, and 3 remain readable;
// every successful write uses version 5.
inline std::string encode_snapshot(const ScheduleSnapshot& snapshot) {
    if (!valid_snapshot(snapshot)) return {};
    std::string data = "POWER_TIMER 5\n" + std::to_string(snapshot.next_id) + " " +
        std::to_string(snapshot.next_occurrence_id) + " " + std::to_string(snapshot.records.size()) + " " +
        (snapshot.sound_enabled ? "1" : "0") + "\n" +
        std::to_string(snapshot.theme) + "\n" +
        std::to_string(snapshot.locale) + "\n";
    for (const auto& record : snapshot.records) {
        data += std::to_string(record.id) + " " + std::to_string(record.occurrence_id) + " " +
            std::to_string(static_cast<unsigned>(record.action)) + " " + std::to_string(record.minutes) + " " +
            std::to_string(record.due_utc_ms) + " " + (record.paused ? "1" : "0") + " " +
            std::to_string(static_cast<unsigned>(record.state)) + " " +
            std::to_string(static_cast<unsigned>(record.kind)) + " " +
            std::to_string(record.civil.year) + " " + std::to_string(record.civil.month) + " " +
            std::to_string(record.civil.day) + " " + std::to_string(record.civil.hour) + " " +
            std::to_string(record.civil.minute) + " " + std::to_string(record.weekdays) + " " +
            std::to_string(record.minute_of_day) + " " + std::to_string(record.civil_high_water) + " " +
            std::to_string(record.last_occurrence_id) + " " +
            std::to_string(static_cast<unsigned>(record.last_state)) + " " +
            std::to_string(record.conflict_with) + " " + (record.force_shutdown ? "1" : "0") + " " +
            std::to_string(record.postponed_utc_ms) + "\n";
    }
    return data.size() <= max_schedule_file_bytes ? data : std::string{};
}

inline std::optional<ScheduleSnapshot> decode_snapshot(std::string_view data) {
    constexpr std::string_view version_1 = "POWER_TIMER 1\n";
    constexpr std::string_view version_2 = "POWER_TIMER 2\n";
    constexpr std::string_view version_3 = "POWER_TIMER 3\n";
    constexpr std::string_view version_4 = "POWER_TIMER 4\n";
    constexpr std::string_view version_5 = "POWER_TIMER 5\n";
    if (data.size() > max_schedule_file_bytes) return std::nullopt;
    const bool legacy = data.starts_with(version_1);
    const bool v2 = data.starts_with(version_2);
    const bool v3 = data.starts_with(version_3);
    const bool v5 = data.starts_with(version_5);
    const bool v4 = data.starts_with(version_4) || v5; // v5 is v4 plus a per-record postponement
    if (!legacy && !v2 && !v3 && !v4) return std::nullopt;
    data.remove_prefix(legacy ? version_1.size() : v2 ? version_2.size() : v3 ? version_3.size() : version_4.size());
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
    const auto next_id = number(' '), next_occurrence = number(' '), count = number(v4 ? ' ' : '\n');
    std::optional<std::uint64_t> sound_enabled{};
    if (v4) {
        sound_enabled = number('\n');
        if (!sound_enabled || *sound_enabled > 1) return std::nullopt;
    }
    if (!next_id || !next_occurrence || !count || *count > max_schedule_records) return std::nullopt;
    ScheduleSnapshot snapshot{*next_id, *next_occurrence, {}};
    if (sound_enabled) snapshot.sound_enabled = *sound_enabled == 1;
    // Skip any leading newlines before the theme
    while (!data.empty() && data[0] == '\n') {
        data.remove_prefix(1);
    }
    if (v4) {
        auto theme_val = number('\n');
        if (!theme_val || *theme_val > 2) return std::nullopt;
        snapshot.theme = *theme_val;
        auto locale_val = number('\n');
        if (!locale_val || *locale_val > 1) return std::nullopt;
        snapshot.locale = *locale_val;
    }
    for (std::uint64_t i = 0; i < *count; ++i) {
        const auto id = number(' '), occurrence = number(' '), action = number(' '), minutes = number(' '),
            due = number(' '), paused = number(' '), state = number(legacy ? '\n' : ' ');
        if (!id || !occurrence || !action || !minutes || !due || !paused || !state || *paused > 1 || *action > 2 || *state > 4) return std::nullopt;
        ScheduleRecord record{*id, *occurrence, static_cast<Action>(*action), *minutes,
            *due, *paused == 1, static_cast<OccurrenceState>(*state)};
        if (!legacy) {
            const auto kind = number(' '), year = number(' '), month = number(' '), day = number(' '),
                hour = number(' '), minute = number(' '), weekdays = number(' '), minute_of_day = number(' '),
                high_water = number(' '), last_id = number(' '), last_state = number((v3 || v4) ? ' ' : '\n');
            if (!kind || !year || !month || !day || !hour || !minute || !weekdays || !minute_of_day ||
                !high_water || !last_id || !last_state || *kind > 2 || *last_state > 4 ||
                *year > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
                *month > std::numeric_limits<unsigned>::max() || *day > std::numeric_limits<unsigned>::max() ||
                *hour > std::numeric_limits<unsigned>::max() || *minute > std::numeric_limits<unsigned>::max() ||
                *weekdays > std::numeric_limits<unsigned>::max() ||
                *minute_of_day > std::numeric_limits<unsigned>::max()) return std::nullopt;
            record.kind = static_cast<ScheduleKind>(*kind);
            record.civil = {static_cast<int>(*year), static_cast<unsigned>(*month),
                static_cast<unsigned>(*day), static_cast<unsigned>(*hour), static_cast<unsigned>(*minute)};
            record.weekdays = static_cast<unsigned>(*weekdays);
            record.minute_of_day = static_cast<unsigned>(*minute_of_day);
            record.civil_high_water = *high_water;
            record.last_occurrence_id = *last_id;
            record.last_state = static_cast<OccurrenceState>(*last_state);
            if (v3 || v4) {
                const auto conflict_with = number(' '), force_shutdown = number(v5 ? ' ' : '\n');
                if (!conflict_with || !force_shutdown || *force_shutdown > 1) return std::nullopt;
                if (v5) {
                    const auto postponed = number('\n');
                    if (!postponed) return std::nullopt;
                    record.postponed_utc_ms = *postponed;
                }
                record.conflict_with = *conflict_with;
                record.force_shutdown = *force_shutdown == 1;
            }
        }
        snapshot.records.push_back(record);
    }
    if (!data.empty() || !valid_snapshot(snapshot)) return std::nullopt;
    return snapshot;
}
} // namespace power_timer
