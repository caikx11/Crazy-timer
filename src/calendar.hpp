#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace power_timer {

struct CivilTime {
    int year;
    unsigned month, day, hour, minute;
    bool operator==(const CivilTime&) const = default;
};

class Calendar {
public:
    virtual ~Calendar() = default;
    virtual std::optional<CivilTime> local(std::uint64_t utc_ms) const = 0;
    // nullopt = adapter failure; empty vector = skipped local time (DST gap).
    virtual std::optional<std::vector<std::uint64_t>> resolve(CivilTime civil) const = 0;
};

inline bool valid_civil(CivilTime civil) {
    using namespace std::chrono;
    return civil.year >= 1970 && civil.year <= 9999 &&
           civil.month >= 1 && civil.month <= 12 && civil.day >= 1 && civil.day <= 31 &&
           civil.hour < 24 && civil.minute < 60 &&
           year_month_day{year{civil.year}, month{civil.month}, day{civil.day}}.ok();
}

inline std::uint64_t civil_key(CivilTime civil) {
    if (!valid_civil(civil)) return 0;
    return static_cast<std::uint64_t>(civil.year) * 100000000ULL +
           civil.month * 1000000ULL + civil.day * 10000ULL +
           civil.hour * 100ULL + civil.minute;
}

inline std::optional<CivilTime> civil_from_key(std::uint64_t key) {
    if (key / 100000000ULL < 1970 || key / 100000000ULL > 9999) return std::nullopt;
    const CivilTime civil{
        static_cast<int>(key / 100000000ULL),
        static_cast<unsigned>((key / 1000000ULL) % 100),
        static_cast<unsigned>((key / 10000ULL) % 100),
        static_cast<unsigned>((key / 100ULL) % 100),
        static_cast<unsigned>(key % 100)};
    if (!valid_civil(civil)) return std::nullopt;
    return civil;
}

inline std::optional<CivilTime> next_day(CivilTime civil) {
    using namespace std::chrono;
    if (!valid_civil(civil)) return std::nullopt;
    const auto next = sys_days{year{civil.year}/month{civil.month}/day{civil.day}} + days{1};
    const year_month_day date{next};
    const CivilTime result{int(date.year()), unsigned(date.month()), unsigned(date.day()),
                           civil.hour, civil.minute};
    if (!valid_civil(result)) return std::nullopt;
    return result;
}

inline unsigned weekday(CivilTime civil) {
    using namespace std::chrono;
    if (!valid_civil(civil)) return 0;
    return std::chrono::weekday{sys_days{year{civil.year}/month{civil.month}/day{civil.day}}}.c_encoding();
}

} // namespace power_timer
