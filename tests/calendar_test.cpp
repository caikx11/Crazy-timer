#include "../src/calendar.hpp"
#include "../src/windows/local_calendar.hpp"
#include <cstdlib>
#include <iostream>

using namespace power_timer;

int checks = 0;
void check(bool value, const char* description) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}

void civil_boundaries() {
    check(valid_civil({2000, 2, 29, 23, 59}), "century leap day accepted");
    check(!valid_civil({1900, 2, 29, 12, 0}) && !valid_civil({2100, 2, 29, 12, 0}),
          "non-leap centuries rejected");
    check(!valid_civil({1969, 12, 31, 0, 0}) && !valid_civil({10000, 1, 1, 0, 0}),
          "supported civil years are bounded");
    check(!valid_civil({2026, 4, 31, 0, 0}) && !valid_civil({2026, 1, 1, 24, 0}) &&
          !valid_civil({2026, 1, 1, 23, 60}), "invalid days and clock values rejected");
    check(!valid_civil({2026, 257, 1, 12, 0}) && !valid_civil({2026, 1, 257, 12, 0}),
          "large month and day values cannot wrap in chrono fields");
    check(civil_key({2026, 10, 8, 9, 5}) == 202610080905ULL,
          "civil key has YYYYMMDDHHMM shape");
    check(civil_from_key(202610080905ULL) == CivilTime{2026, 10, 8, 9, 5} &&
          !civil_from_key(202602300905ULL), "civil keys decode only valid dates");
    check(!civil_from_key(429497132001010000ULL),
          "oversized civil key cannot wrap its year into the accepted range");
    check(next_day({2024, 2, 28, 18, 45}) == CivilTime{2024, 2, 29, 18, 45} &&
          next_day({2024, 12, 31, 18, 45}) == CivilTime{2025, 1, 1, 18, 45},
          "next day handles leap days and year turn");
    check(!next_day({9999, 12, 31, 0, 0}), "next day stops at supported end");
    check(weekday({2026, 10, 4, 0, 0}) == 0 && weekday({1970, 1, 1, 0, 0}) == 4,
          "weekday begins with Sunday zero");
}

DYNAMIC_TIME_ZONE_INFORMATION eastern_zone() {
    DYNAMIC_TIME_ZONE_INFORMATION zone{};
    zone.Bias = 300;
    zone.StandardBias = 0;
    zone.DaylightBias = -60;
    zone.StandardDate.wMonth = 11;
    zone.StandardDate.wDay = 1;
    zone.StandardDate.wDayOfWeek = 0;
    zone.StandardDate.wHour = 2;
    zone.DaylightDate.wMonth = 3;
    zone.DaylightDate.wDay = 2;
    zone.DaylightDate.wDayOfWeek = 0;
    zone.DaylightDate.wHour = 2;
    return zone;
}

void eastern_dst_boundaries() {
    const WindowsCalendar calendar{eastern_zone()};
    const Calendar& through_interface = calendar;
    check(through_interface.resolve({2026, 3, 8, 2, 30}) ==
          std::optional<std::vector<std::uint64_t>>{{}},
          "spring gap is an empty successful resolution");
    check(through_interface.resolve({2026, 11, 1, 1, 30}) ==
          std::optional<std::vector<std::uint64_t>>{{1793511000000ULL, 1793514600000ULL}},
          "autumn fold returns both UTC instants in order");
    check(through_interface.local(1793511000000ULL) == CivilTime{2026, 11, 1, 1, 30} &&
          through_interface.local(1793514600000ULL) == CivilTime{2026, 11, 1, 1, 30},
          "both UTC fold instants render the same civil minute");
}

void fixed_zone_and_live_zone() {
    DYNAMIC_TIME_ZONE_INFORMATION fixed{};
    fixed.Bias = -120; // UTC+02:00, no seasonal transition.
    const WindowsCalendar plus_two{fixed};
    check(plus_two.resolve({2026, 1, 1, 1, 30}) ==
          std::optional<std::vector<std::uint64_t>>{{1767223800000ULL}},
          "fixed zone resolves civil time across UTC year turn");
    check(plus_two.local(1767223800000ULL) == CivilTime{2026, 1, 1, 1, 30},
          "fixed zone renders a civil date in the following year");

    const WindowsCalendar utc{DYNAMIC_TIME_ZONE_INFORMATION{}};
    check(utc.local(0) == CivilTime{1970, 1, 1, 0, 0} &&
          utc.resolve({1970, 1, 1, 0, 0}) ==
              std::optional<std::vector<std::uint64_t>>{{0}},
          "Unix epoch is within the supported civil range");
    check(utc.local(253402300799999ULL) == CivilTime{9999, 12, 31, 23, 59} &&
          utc.resolve({9999, 12, 31, 23, 59}) ==
              std::optional<std::vector<std::uint64_t>>{{253402300740000ULL}},
          "last supported civil minute resolves without overflow");
    check(!utc.local(253402300800000ULL), "UTC instant in year 10000 rejected");

    const WindowsCalendar live;
    const auto current_utc = 1793511000000ULL;
    const auto civil = live.local(current_utc);
    check(civil.has_value(), "current Windows zone converts a supported UTC instant");
    const auto candidates = live.resolve(*civil);
    check(candidates.has_value() &&
          std::find(candidates->begin(), candidates->end(), current_utc) != candidates->end(),
          "current Windows zone round-trips a civil minute");
}

void disabled_daylight_uses_base_bias() {
    auto zone = eastern_zone();
    zone.StandardBias = 30;
    zone.DynamicDaylightTimeDisabled = TRUE;
    const WindowsCalendar calendar{zone};
    check(calendar.resolve({2026, 7, 1, 12, 0}) ==
          std::optional<std::vector<std::uint64_t>>{{1782925200000ULL}},
          "disabled daylight uses base bias during summer");
    check(calendar.resolve({2026, 1, 1, 12, 0}) ==
          std::optional<std::vector<std::uint64_t>>{{1767286800000ULL}},
          "disabled daylight ignores standard and daylight biases in winter");
}

int main() {
    civil_boundaries();
    eastern_dst_boundaries();
    fixed_zone_and_live_zone();
    disabled_daylight_uses_base_bias();
    std::cout << checks << " calendar checks passed\n";
}
