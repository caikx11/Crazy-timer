#pragma once

#include "../calendar.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>

namespace power_timer {

class WindowsCalendar final : public Calendar {
public:
    WindowsCalendar() = default;
    explicit WindowsCalendar(const DYNAMIC_TIME_ZONE_INFORMATION& zone) : fixed_zone_(zone) {}

    std::optional<CivilTime> local(std::uint64_t utc_ms) const override {
        const auto zone = snapshot();
        if (!zone) return std::nullopt;
        return local_with_zone(utc_ms, *zone);
    }

    std::optional<std::vector<std::uint64_t>> resolve(CivilTime civil) const override {
        if (!valid_civil(civil)) return std::nullopt;
        const auto zone = snapshot();
        if (!zone) return std::nullopt;

        TIME_ZONE_INFORMATION annual{};
        if (zone->TimeZoneKeyName[0] && !zone->DynamicDaylightTimeDisabled) {
            // GetTimeZoneInformationForYear requires the *local* year. Microsoft Learn:
            // https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-gettimezoneinformationforyear
            auto mutable_zone = *zone;
            if (!GetTimeZoneInformationForYear(static_cast<USHORT>(civil.year), &mutable_zone, &annual))
                return std::nullopt;
        } else {
            annual.Bias = zone->Bias;
            annual.StandardBias = zone->StandardBias;
            annual.DaylightBias = zone->DaylightBias;
            annual.StandardDate = zone->StandardDate;
            annual.DaylightDate = zone->DaylightDate;
        }

        // DYNAMIC_TIME_ZONE_INFORMATION defines UTC = local + Bias; the two
        // annual biases cover both sides of a repeated hour. Microsoft Learn:
        // https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/ns-timezoneapi-dynamic_time_zone_information
        // TzSpecificLocalTimeToSystemTimeEx chooses only one side of a fold and
        // can map an invalid gap. Round-trip each candidate instead:
        // https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-tzspecificlocaltimetosystemtimeex
        // https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltimeex
        const auto naive = naive_utc_ms(civil);
        std::vector<std::uint64_t> result;
        const auto try_bias = [&](std::int64_t bias) -> bool {
            const std::int64_t shifted = naive + bias * 60000;
            if (shifted < 0 || shifted >= last_utc_ms) return true;
            const auto candidate = static_cast<std::uint64_t>(shifted);
            const auto roundtrip = local_with_zone(candidate, *zone);
            if (!roundtrip) return false;
            if (*roundtrip == civil) result.push_back(candidate);
            return true;
        };
        const bool has_transitions = annual.StandardDate.wMonth && annual.DaylightDate.wMonth &&
                                     !zone->DynamicDaylightTimeDisabled;
        const std::int64_t standard_bias = static_cast<std::int64_t>(annual.Bias) +
                                           (has_transitions ? annual.StandardBias : 0);
        if (!try_bias(standard_bias))
            return std::nullopt;
        if (has_transitions &&
            !try_bias(static_cast<std::int64_t>(annual.Bias) + annual.DaylightBias))
            return std::nullopt;
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

private:
    // Unix milliseconds for 10000-01-01T00:00Z; upper bound is exclusive.
    static constexpr std::int64_t last_utc_ms = 253402300800000LL;
    static constexpr std::uint64_t filetime_epoch_ticks = 116444736000000000ULL;
    std::optional<DYNAMIC_TIME_ZONE_INFORMATION> fixed_zone_;

    std::optional<DYNAMIC_TIME_ZONE_INFORMATION> snapshot() const {
        if (fixed_zone_) return *fixed_zone_;
        DYNAMIC_TIME_ZONE_INFORMATION zone{};
        // Captured once per public call, so a time-zone change cannot split a
        // resolve operation. TIME_ZONE_ID_INVALID alone signals API failure.
        // https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-getdynamictimezoneinformation
        if (GetDynamicTimeZoneInformation(&zone) == TIME_ZONE_ID_INVALID) return std::nullopt;
        return zone;
    }

    static std::int64_t naive_utc_ms(CivilTime civil) {
        using namespace std::chrono;
        return duration_cast<milliseconds>(sys_days{year{civil.year}/month{civil.month}/day{civil.day}}.time_since_epoch()).count() +
               static_cast<std::int64_t>(civil.hour) * 3600000 +
               static_cast<std::int64_t>(civil.minute) * 60000;
    }

    static std::optional<CivilTime> local_with_zone(
        std::uint64_t utc_ms, const DYNAMIC_TIME_ZONE_INFORMATION& zone) {
        if (utc_ms >= static_cast<std::uint64_t>(last_utc_ms)) return std::nullopt;
        const std::uint64_t ticks = filetime_epoch_ticks + utc_ms * 10000;
        const FILETIME filetime{static_cast<DWORD>(ticks), static_cast<DWORD>(ticks >> 32)};
        SYSTEMTIME utc{};
        if (!FileTimeToSystemTime(&filetime, &utc)) return std::nullopt;
        SYSTEMTIME local_time{};
        if (!zone.TimeZoneKeyName[0] || zone.DynamicDaylightTimeDisabled) {
            // An explicit test zone has no registry key. The legacy converter
            // accepts its transition rules directly and does not read system
            // time-zone state. Live zones use the dynamic converter below to
            // avoid the legacy converter's documented cross-year limitation:
            // https://learn.microsoft.com/en-us/windows/win32/api/timezoneapi/nf-timezoneapi-systemtimetotzspecificlocaltime
            TIME_ZONE_INFORMATION rules{};
            rules.Bias = zone.Bias;
            rules.StandardBias = zone.StandardBias;
            rules.DaylightBias = zone.DaylightBias;
            rules.StandardDate = zone.StandardDate;
            rules.DaylightDate = zone.DaylightDate;
            if (zone.DynamicDaylightTimeDisabled) {
                rules.StandardDate = {};
                rules.DaylightDate = {};
            }
            if (!SystemTimeToTzSpecificLocalTime(&rules, &utc, &local_time)) return std::nullopt;
        } else {
            // Windhawk's MinGW headers omit this Windows 7+ declaration,
            // though kernel32 exports it. Resolve it at runtime.
            using Convert = BOOL (WINAPI *)(const DYNAMIC_TIME_ZONE_INFORMATION*,
                                             const SYSTEMTIME*, LPSYSTEMTIME);
            const auto module = GetModuleHandleW(L"kernel32.dll");
            if (!module) return std::nullopt;
            const auto convert = reinterpret_cast<Convert>(
                GetProcAddress(module, "SystemTimeToTzSpecificLocalTimeEx"));
            if (!convert || !convert(&zone, &utc, &local_time)) return std::nullopt;
        }
        const CivilTime result{local_time.wYear, local_time.wMonth, local_time.wDay,
                               local_time.wHour, local_time.wMinute};
        if (!valid_civil(result)) return std::nullopt;
        return result;
    }
};

} // namespace power_timer
