#include "../src/schedule_book.hpp"
#include "../src/power_executor.hpp"
#include <cstdlib>
#include <iostream>
#include <chrono>

using namespace power_timer;
int checks = 0;
void check(bool value, const char* description) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << description << '\n'; std::exit(1); }
}
struct MemoryStorage : ScheduleStorage {
    LoadResult data{LoadStatus::Missing, {}};
    bool fail_save = false;
    LoadResult load() override { return data; }
    bool save(const ScheduleSnapshot& value) override {
        if (fail_save || !valid_snapshot(value)) return false;
        data = {LoadStatus::Loaded, value};
        return true;
    }
};
struct TestCalendar : Calendar {
    int offset_minutes = 0;
    bool unavailable = false;
    bool all_gaps = false;
    std::map<std::uint64_t, std::vector<std::uint64_t>> special;
    std::optional<CivilTime> local(std::uint64_t utc) const override {
        using namespace std::chrono;
        if (unavailable || utc >= 253402300800000ULL) return {};
        const auto point = sys_time<milliseconds>(milliseconds(utc)) + minutes(offset_minutes);
        const auto day = floor<days>(point);
        const year_month_day date(day);
        const hh_mm_ss time(point - day);
        CivilTime result{int(date.year()), unsigned(date.month()), unsigned(date.day()),
            unsigned(time.hours().count()), unsigned(time.minutes().count())};
        return valid_civil(result) ? std::optional(result) : std::nullopt;
    }
    std::optional<std::vector<std::uint64_t>> resolve(CivilTime civil) const override {
        using namespace std::chrono;
        if (unavailable || !valid_civil(civil)) return {};
        if (all_gaps) return std::vector<std::uint64_t>{};
        const auto found = special.find(civil_key(civil));
        if (found != special.end()) return found->second;
        const auto point = sys_days(year(civil.year)/month(civil.month)/day(civil.day)) +
            hours(civil.hour) + minutes(civil.minute) - minutes(offset_minutes);
        const auto value = duration_cast<milliseconds>(point.time_since_epoch()).count();
        if (value < 0) return std::vector<std::uint64_t>{};
        return std::vector<std::uint64_t>{static_cast<std::uint64_t>(value)};
    }
};
void postpone_moves_only_the_current_occurrence() {
    {
        MemoryStorage storage;
        ScheduleBook book(storage);
        check(book.initialize({0, 0}) && book.create(Action::Sleep, L"15", {0, 0}), "prepare duration postpone");
        const auto occurrence = book.list()[0].occurrence_id;
        check(book.postpone(1, 10, {0, 0}) && book.remaining_ms(1, {0, 0}) == 1500000 &&
              book.list()[0].due_utc_ms == 1500000 && book.list()[0].occurrence_id == occurrence,
              "postpone keeps occurrence identity and extends deadline");
        ScheduleBook reopened(storage);
        check(reopened.initialize({0, 0}) && reopened.remaining_ms(1, {0, 0}) == 1500000, "postponement survives restart");
        check(!book.postpone(1, 0, {0, 0}) && book.error() == BookError::InvalidInput, "zero minutes rejected");
        check(!book.postpone(9, 10, {0, 0}) && book.error() == BookError::NotFound, "unknown schedule rejected");
        check(book.create(Action::Hibernate, L"40", {0, 0}), "prepare postpone collision");
        check(!book.postpone(1, 15, {0, 0}) && book.error() == BookError::Conflict && book.conflicting_id() == 2 &&
              book.remaining_ms(1, {0, 0}) == 1500000, "colliding postpone keeps original deadline");
        check(!book.postpone(1, 10, {1500000, 1500000}) && book.error() == BookError::InvalidInput,
              "no postpone at the deadline: tick owns the single intent");
        check(book.tick({1500000, 1500000}).size() == 1, "deadline still delivers exactly once");
    }
    {
        MemoryStorage storage;
        TestCalendar calendar;
        ScheduleBook book(storage, calendar);
        check(book.initialize({0, 0}) && book.create_weekly(Action::Sleep, 127, 1, {0, 0}), "prepare weekly postpone");
        const auto occurrence = book.list()[0].occurrence_id;
        check(book.postpone(1, 10, {0, 0}) && book.list()[0].due_utc_ms == 60000 &&
              book.list()[0].minute_of_day == 1 && book.list()[0].occurrence_id == occurrence &&
              book.remaining_ms(1, {0, 0}) == 660000, "weekly postpone keeps usual rule time");
        ScheduleBook reopened(storage, calendar);
        check(reopened.initialize({0, 0}) && reopened.remaining_ms(1, {0, 0}) == 660000, "weekly postponement survives restart");
        check(book.tick({60000, 60000}).empty(), "no delivery at the usual time");
        const auto delivered = book.tick({660000, 660000});
        check(delivered.size() == 1 && book.list()[0].postponed_utc_ms == 0 &&
              book.list()[0].due_utc_ms == 86460000, "delivers once at new time; next day keeps usual time");
    }
}
void once_uses_fixed_instant() {
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage, calendar);
    check(book.initialize({0, 0}), "initialize calendar book");
    check(book.preview_once({1970, 1, 1, 0, 1}, {0, 0}) == 60000 && book.list().empty(),
          "once preview returns future instant without saving");
    check(book.create_once(Action::Sleep, {1970, 1, 1, 0, 1}, {0, 0}), "create explicit date");
    calendar.offset_minutes = 60;
    check(book.reconcile({30000, 30000}) && book.list()[0].due_utc_ms == 60000,
          "once keeps fixed UTC when timezone changes");
    const auto delivery = book.tick({65000, 65000});
    check(delivery.size() == 1 && book.complete(delivery[0]), "once dispatches within tolerance");
    check(!book.create_once(Action::Sleep, {1970, 1, 1, 0, 1}, {70000, 70000}), "past explicit date rejected");
}
void weekly_keeps_rule_after_delivery_and_cancel() {
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage, calendar);
    check(book.initialize({0, 0}) && book.preview_weekly(127, 1, {0, 0}) == 60000,
          "weekly preview finds today's future minute");
    check(book.create_weekly(Action::Hibernate, 127, 1, {0, 0}), "create daily rule");
    const auto delivered = book.tick({60000, 60000});
    check(delivered.size() == 1 && book.list()[0].due_utc_ms == 86460000 &&
          book.list()[0].state == OccurrenceState::Scheduled &&
          book.list()[0].last_state == OccurrenceState::Uncertain,
          "weekly intent consumes today's civil identity and saves tomorrow before delivery");
    check(book.complete(delivered[0]) && book.list()[0].last_state == OccurrenceState::Completed,
          "weekly completion preserves next occurrence");
    check(book.cancel(1) && book.list()[0].due_utc_ms == 172860000 &&
          book.list()[0].last_state == OccurrenceState::Cancelled && book.list()[0].weekdays == 127,
          "cancel consumes just next occurrence and keeps recurrence");
    ScheduleBook restarted(storage, calendar);
    check(restarted.initialize({0, 0}) && restarted.tick({60000, 60000}).empty() &&
          restarted.list()[0].due_utc_ms == 172860000, "clock return and restart never resurrect consumed civil dates");
}
void calendar_failures_block_effects() {
    {
        MemoryStorage storage;
        TestCalendar calendar;
        calendar.all_gaps = true;
        ScheduleBook book(storage,calendar);
        check(book.initialize({0,0}) && !book.create_weekly(Action::Sleep,127,1,{0,0}) &&
            book.error() == BookError::Calendar && book.list().empty(),
            "adapter without any reachable civil time reports unavailable calendar without saving");
    }
    for (const bool weekly : {false, true}) {
        MemoryStorage storage;
        TestCalendar calendar;
        ScheduleBook book(storage, calendar);
        check(book.initialize({0, 0}) && (weekly ? book.create_weekly(Action::Sleep, 127, 1, {0, 0}) :
            book.create_once(Action::Sleep, {1970, 1, 1, 0, 1}, {0, 0})), "prepare calendar outage");
        calendar.unavailable = true;
        check(book.tick({60000, 60000}).empty() && !book.writable() && book.error() == BookError::Calendar,
              "calendar failure blocks intent instead of delivering or silently skipping rule");
        calendar.unavailable = false;
        check(book.initialize({60000, 60000}) && book.tick({60000, 60000}).empty(),
              "recovery from calendar failure expires missed occurrence");
    }
}
void recurring_calendar_boundaries() {
    struct Case { std::uint64_t now, expected; unsigned mask, minute; CivilTime civil; };
    // Known UTC dates: 2023-12-31 Sunday, 2024-01-31 Wednesday, leap day Thursday.
    const Case cases[] = {
        {1704067140000ULL, 1704067200000ULL, 127, 0, {2024,1,1,0,0}},
        {1706745540000ULL, 1706745600000ULL, 127, 0, {2024,2,1,0,0}},
        {1709164740000ULL, 1709164800000ULL, 127, 0, {2024,2,29,0,0}},
        {1709251140000ULL, 1709251200000ULL, 127, 0, {2024,3,1,0,0}},
        {1704067200000ULL, 1704672000000ULL, 2, 0, {2024,1,8,0,0}},
        {1704499200000ULL, 1704672000000ULL, 62, 0, {2024,1,8,0,0}}
    };
    for (const auto& item : cases) {
        MemoryStorage storage;
        TestCalendar calendar;
        ScheduleBook book(storage, calendar);
        check(book.initialize({item.now, 0}) && book.create_weekly(Action::Sleep, item.mask, item.minute, {item.now, 0}),
              "create weekly across calendar boundary");
        check(book.list()[0].due_utc_ms == item.expected && book.list()[0].civil == item.civil,
              "day/week/month/leap-year/year boundary selects correct next date");
    }
}
void gaps_and_folds_are_consumed_only_once() {
    MemoryStorage storage;
    TestCalendar calendar;
    calendar.special[197001010001ULL] = {};
    ScheduleBook book(storage, calendar);
    check(book.initialize({0, 0}) && !book.create_once(Action::Sleep, {1970,1,1,0,1}, {0,0}) &&
          book.error() == BookError::InvalidInput, "nonexistent once time is rejected");
    check(book.create_weekly(Action::Sleep, 127, 1, {0,0}) && book.list()[0].due_utc_ms == 86460000 &&
          book.list()[0].civil_high_water == 197001010001ULL, "weekly skips nonexistent time and records skipped identity");
    calendar.special.clear();
    check(book.reconcile({0,0}) && book.list()[0].due_utc_ms == 86460000,
          "timezone return cannot revive a skipped civil time");
    calendar.special[197001010002ULL] = {120000, 3720000};
    check(book.edit_once(1, Action::Sleep, {1970,1,1,0,2}, {0,0}) && book.list()[0].due_utc_ms == 120000,
          "ambiguous once resolves only earliest instant");
    check(!book.create_once(Action::Sleep, {1970,1,1,0,2}, {120001,0}),
          "second fold cannot turn a past once selection into future");
    check(book.edit_weekly(1, Action::Hibernate, 127, 2, {0,0}), "edit to folded weekly time");
    const auto delivered = book.tick({120000,120000});
    check(delivered.size() == 1 && book.tick({3720000,3720000}).empty() &&
          book.list()[0].due_utc_ms == 86520000, "second fold never repeats the civil occurrence");
    ScheduleBook restarted(storage, calendar);
    check(restarted.initialize({0,0}) && restarted.list()[0].last_state == OccurrenceState::Uncertain &&
          restarted.tick({120000,120000}).empty(), "uncertain folded result survives restart without resending");
}
void weekly_clock_changes_and_pauses() {
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage, calendar);
    check(book.initialize({0,0}) && book.create_weekly(Action::Sleep, 127, 120, {0,0}), "prepare timezone change");
    calendar.offset_minutes = 180;
    check(book.reconcile({0,0}) && book.list()[0].due_utc_ms == 82800000 &&
          book.list()[0].last_state == OccurrenceState::Expired, "timezone advance expires skipped local occurrence");
    calendar.offset_minutes = 0;
    check(book.reconcile({0,0}) && book.list()[0].due_utc_ms == 93600000,
          "timezone return keeps consumed date suppressed and recalculates next instant");
    check(book.set_paused(1,true,{0,0}) && book.tick({93600000,93600000}).empty() &&
          book.list()[0].last_state == OccurrenceState::Expired && book.list()[0].due_utc_ms == 180000000,
          "paused weekly occurrence expires while recurrence advances");
    check(book.set_paused(1,false,{180000001,180000001}) && book.list()[0].due_utc_ms == 266400000 &&
          book.tick({180000001,180000001}).empty(), "reactivation expires missed weekly occurrence without delivery");
    check(book.reconcile({900000000,900000000}) && book.list()[0].due_utc_ms == 957600000,
          "large forward clock change advances directly to next valid date");
    ScheduleBook restarted(storage,calendar);
    check(restarted.initialize({0,0}) && restarted.list()[0].due_utc_ms == 957600000,
          "persistent high water prevents replay after large clock rollback");
}
void recurring_failures_are_transactional() {
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage, calendar);
    check(book.initialize({0,0}) && book.create_weekly(Action::Sleep,127,1,{0,0}), "prepare transactional weekly");
    storage.fail_save = true;
    check(!book.edit_weekly(1,Action::Shutdown,62,3,{0,0}) && !book.cancel(1) &&
          !book.set_paused(1,true,{0,0}) && book.list()[0].weekdays == 127 &&
          book.list()[0].occurrence_id == 1 && !book.list()[0].paused, "failed weekly commands preserve all saved rule fields");
    check(book.tick({60000,60000}).empty() && !book.writable() && book.list()[0].last_occurrence_id == 0 &&
          book.list()[0].due_utc_ms == 60000, "failed weekly intent never advances memory or emits action");
    storage.fail_save = false;
    check(book.initialize({60000,0}) && book.list()[0].last_state == OccurrenceState::Expired &&
          book.list()[0].due_utc_ms == 86460000, "recovery expires failed intent and preserves next rule");
    const auto delivery = book.tick({86460000,86400000});
    check(delivery.size() == 1, "next occurrence remains deliverable after failed intent");
    storage.fail_save = true;
    check(!book.complete(delivery[0]) && !book.writable() && book.list()[0].last_state == OccurrenceState::Uncertain,
          "failed weekly completion retains uncertain result");
    storage.fail_save = false;
    ScheduleBook restarted(storage,calendar);
    check(restarted.initialize({86460001,0}) && restarted.list()[0].last_state == OccurrenceState::Uncertain &&
          restarted.list()[0].due_utc_ms == 172860000 && restarted.tick({86460001,0}).empty(),
          "restart preserves uncertain history and schedules next date without retry");
}
void calendar_validation_and_limit_failures() {
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage,calendar);
    check(book.initialize({0,0}) && book.create_weekly(Action::Sleep,127,1,{0,0}), "prepare calendar validation");
    for (const auto civil : {CivilTime{1969,12,31,23,59}, CivilTime{10000,1,1,0,0},
         CivilTime{2025,2,29,0,0}, CivilTime{2024,2,30,0,0}, CivilTime{2024,1,1,24,0}, CivilTime{2024,1,1,0,60}})
        check(!book.create_once(Action::Sleep,civil,{0,0}) && !book.edit_once(1,Action::Sleep,civil,{0,0}) &&
            book.error() == BookError::InvalidInput && book.list()[0].occurrence_id == 1,
            "invalid civil date never replaces saved occurrence");
    for (const auto mask : {0U,128U,255U})
        check(!book.create_weekly(Action::Sleep,mask,1,{0,0}) && !book.edit_weekly(1,Action::Sleep,mask,1,{0,0}),
              "invalid weekday mask rejected");
    check(!book.create_weekly(Action::Sleep,127,1440,{0,0}) &&
          !book.create_weekly(static_cast<Action>(999),127,1,{0,0}) &&
          !book.create_once(static_cast<Action>(999),{1970,1,1,0,1},{0,0}), "invalid time/action rejected");
    check(!book.edit_once(99,Action::Sleep,{1970,1,1,0,1},{0,0}) && book.error() == BookError::NotFound &&
          !book.edit_weekly(99,Action::Sleep,127,1,{0,0}) && book.error() == BookError::NotFound,
          "missing calendar edit does not create a schedule");
    check(!book.preview_weekly(127,0,{253402300740000ULL,0}) &&
          !book.create_weekly(Action::Sleep,127,0,{253402300740000ULL,0}), "end of supported calendar cannot wrap to year 10000");
    check(!book.preview_once({9999,12,31,23,59},{253402300800000ULL,0}), "out of range current epoch rejected");
    auto valid = storage.data;
    for (int corruption = 0; corruption < 11; ++corruption) {
        storage.data = valid;
        auto& row = storage.data.snapshot.records[0];
        switch (corruption) {
        case 0: row.kind = static_cast<ScheduleKind>(99); break;
        case 1: row.weekdays = 0; break;
        case 2: row.minute_of_day = 1440; break;
        case 3: row.civil = {2025,2,29,0,1}; break;
        case 4: row.civil_high_water = civil_key(row.civil); break;
        case 5: row.last_occurrence_id = row.occurrence_id; break;
        case 6: row.last_state = OccurrenceState::Uncertain; break;
        case 7: row.minutes = 1; break;
        case 8: row.due_utc_ms = 253402300800000ULL; break;
        case 9: row.state = OccurrenceState::Completed; break;
        case 10: row.civil_high_water = 0; break;
        }
        ScheduleBook damaged(storage,calendar);
        check(!damaged.initialize({0,0}) && damaged.error() == BookError::InvalidData && damaged.list().empty(),
              "malformed calendar snapshot blocks recovery without overwriting");
    }
    storage.data = valid;
    storage.data.snapshot.next_occurrence_id = std::numeric_limits<std::uint64_t>::max();
    ScheduleBook exhausted(storage,calendar);
    check(exhausted.initialize({0,0}) && !exhausted.cancel(1) && exhausted.writable() &&
          exhausted.list()[0].occurrence_id == 1, "exhausted occurrence counter rejects cancellation transactionally");
    check(exhausted.tick({60000,60000}).empty() && !exhausted.writable() && exhausted.list()[0].last_occurrence_id == 0,
          "exhausted occurrence counter blocks delivery without wrapping");
    ScheduleBook no_calendar(storage);
    check(!no_calendar.initialize({0,0}) && no_calendar.error() == BookError::Calendar,
          "calendar records cannot silently run without calendar dependency");
}
void editing_modes_and_recovery_tolerance() {
    for (const bool weekly : {false,true}) {
        for (const auto delay : {0ULL,5000ULL,5001ULL}) {
            MemoryStorage storage;
            TestCalendar calendar;
            ScheduleBook book(storage,calendar);
            check(book.initialize({0,0}) && (weekly ? book.create_weekly(Action::Sleep,127,1,{0,0}) :
                book.create_once(Action::Sleep,{1970,1,1,0,1},{0,0})), "prepare civil dispatch boundary");
            const auto delivered = book.tick({60000+delay,60000+delay});
            check(delivered.size() == (delay <= 5000 ? 1U : 0U), "civil dispatch tolerates 5000 ms but expires 5001 ms");
            if (weekly) check(book.list()[0].due_utc_ms == 86460000, "late civil dispatch preserves weekly continuation");
        }
        MemoryStorage storage;
        TestCalendar calendar;
        ScheduleBook book(storage,calendar);
        check(book.initialize({0,0}) && (weekly ? book.create_weekly(Action::Sleep,127,1,{0,0}) :
              book.create_once(Action::Sleep,{1970,1,1,0,1},{0,0})), "prepare civil recovery boundary");
        ScheduleBook reopened(storage,calendar);
        check(reopened.initialize({60000,0}) && reopened.tick({60000,0}).empty(),
              "startup at civil deadline expires even inside dispatch tolerance");
        check(book.reconcile({60000,60000}) && book.tick({60000,60000}).empty(),
              "explicit resume at civil deadline expires without delivery");
        check(book.set_paused(1,true,{60000,60000}) && book.edit(1,Action::Shutdown,L"1",{60000,60000}) &&
              book.list()[0].kind == ScheduleKind::Duration && book.list()[0].last_occurrence_id == 0 && book.list()[0].paused,
              "editing civil rule to duration resets recurrence metadata and keeps pause");
        check(book.edit_once(1,Action::Sleep,{1970,1,1,0,3},{60000,60000}) &&
              book.list()[0].kind == ScheduleKind::Once && book.list()[0].paused,
              "editing duration to once keeps pause and replaces monotonic deadline");
        check(book.edit_weekly(1,Action::Sleep,127,1,{0,0}) && book.list()[0].due_utc_ms == 60000,
              "explicit weekly edit resets high water to create fresh rule identity");
    }
}
void create_and_restore() {
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(book.initialize({100000, 500}), "initialize empty book");
    check(book.create(Action::Sleep, L"2", {100000, 500}), "create duration");
    check(book.list().size() == 1 && book.list()[0].due_utc_ms == 220000,
          "list shows saved duration");
    ScheduleBook reopened(storage);
    check(reopened.initialize({130000, 20}), "restore before deadline");
    check(reopened.list()[0].action == Action::Sleep &&
          reopened.remaining_ms(1, {130000, 20}) == 90000,
          "restore counts elapsed time while closed");
}
void manage_list_transactionally() {
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(book.initialize({100000, 0}), "initialize command flow");
    check(book.create(Action::Sleep, L"1", {100000, 0}), "create first item");
    check(book.create(Action::Hibernate, L"2", {100000, 0}), "create second item");
    check(book.set_paused(1, true, {110000, 10000}), "pause selected item");
    check(book.edit(1, Action::Shutdown, L"3", {110000, 10000}), "edit selected item");
    check(book.list()[0].paused && book.list()[0].occurrence_id == 3 &&
          book.list()[0].due_utc_ms == 290000 && book.list()[1].minutes == 2,
          "edit creates new occurrence and preserves pause and other records");
    storage.fail_save = true;
    check(!book.edit(1, Action::Sleep, L"4", {110000, 10000}), "failed edit reported");
    check(!book.set_paused(1, false, {110000, 10000}), "failed pause reported");
    check(!book.erase(1) && !book.cancel(2), "failed erase and cancel reported");
    check(!book.create(Action::Sleep, L"4", {110000, 10000}), "failed create reported");
    check(book.writable() && book.error() == BookError::Storage &&
          book.list().size() == 2 && book.list()[0].paused && book.list()[0].minutes == 3 &&
          book.list()[1].state == OccurrenceState::Scheduled,
          "failed commands preserve observable list and permit explicit retry");
    storage.fail_save = false;
    check(book.cancel(2) && book.erase(1), "cancel and erase selected records");
    ScheduleBook reopened(storage);
    check(reopened.initialize({110000, 0}) && reopened.list().size() == 1 &&
          reopened.list()[0].id == 2 && reopened.list()[0].state == OccurrenceState::Cancelled,
          "commands survive reopening");
}
void recover_and_deliver_once() {
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(book.initialize({100000, 1000}) &&
          book.create(Action::Sleep, L"1", {100000, 1000}), "prepare recovery flow");
    check(book.reconcile({130000, 31000}) && book.remaining_ms(1, {130000, 31000}) == 30000,
          "resume before deadline preserves occurrence");
    check(book.tick({999999999, 31000}).empty() &&
          book.remaining_ms(1, {0, 31000}) == 30000, "civil clock jumps preserve monotonic duration");
    const auto delivered = book.tick({1, 61000});
    check(delivered.size() == 1 && delivered[0].id == 1 &&
          book.list()[0].state == OccurrenceState::Uncertain, "consume intent before delivery");
    ScheduleBook crashed(storage);
    check(crashed.initialize({100001, 0}) && crashed.tick({9999999, 9999999}).empty(),
          "crash after intent commit never resends");
    check(book.complete(delivered[0]) && book.list()[0].state == OccurrenceState::Completed &&
          book.tick({200000, 100000}).empty(), "completed occurrence never resends");
    check(book.edit(1, Action::Sleep, L"1", {200000, 100000}), "prepare missed occurrence");
    check(book.reconcile({270000, 170000}) && book.list()[0].state == OccurrenceState::Expired &&
          book.tick({270000, 170000}).empty(), "resume after deadline expires without effect");
    check(book.edit(1, Action::Sleep, L"1", {300000, 200000}), "prepare startup expiration");
    ScheduleBook after_deadline(storage);
    check(after_deadline.initialize({360000, 10}) &&
          after_deadline.list()[0].state == OccurrenceState::Expired &&
          after_deadline.tick({360000, 10}).empty(), "startup at deadline expires before timer");
    check(book.edit(1, Action::Sleep, L"1", {400000, 300000}) &&
          book.set_paused(1, true, {400000, 300000}), "prepare paused deadline");
    check(book.set_paused(1, false, {460000, 360000}) &&
          book.list()[0].state == OccurrenceState::Expired && book.tick({460000, 360000}).empty(),
          "unpausing overdue occurrence never triggers it");
}
void damaged_storage_blocks_processing() {
    for (const auto status : {LoadStatus::Invalid, LoadStatus::Unavailable}) {
        MemoryStorage storage;
        storage.data.status = status;
        ScheduleBook book(storage);
        check(!book.initialize({100000, 0}) && !book.writable(), "bad load blocks book");
        check(book.error() == (status == LoadStatus::Invalid ? BookError::InvalidData : BookError::Storage),
              "bad load has explicit error");
        check(!book.create(Action::Sleep, L"1", {100000, 0}) &&
              book.tick({999999, 999999}).empty() && storage.data.status == status,
              "bad data neither overwritten nor delivered");
    }
    const ScheduleSnapshot valid{2, 2, {{1, 1, Action::Sleep, 1, 160000, false, OccurrenceState::Scheduled}}};
    for (int corruption = 0; corruption < 10; ++corruption) {
        auto data = valid;
        switch (corruption) {
        case 0: data.next_id = 1; break;
        case 1: data.next_occurrence_id = 0; break;
        case 2: data.records[0].id = 0; break;
        case 3: data.records[0].occurrence_id = 0; break;
        case 4: data.records[0].action = static_cast<Action>(99); break;
        case 5: data.records[0].state = static_cast<OccurrenceState>(99); break;
        case 6: data.records[0].minutes = 10081; break;
        case 7: data.next_occurrence_id = 1; break;
        case 8: data.records.push_back(data.records[0]); break;
        case 9: data.records[0].minutes = 0; break;
        }
        MemoryStorage storage;
        storage.data = {LoadStatus::Loaded, data};
        ScheduleBook book(storage);
        check(!book.initialize({100000, 0}) && book.error() == BookError::InvalidData &&
              book.list().empty() && book.tick({999999, 999999}).empty(),
              "invalid loaded snapshot is blocked even behind a storage adapter");
    }
}
void reject_input_and_capacity_without_loss() {
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(book.initialize({100000, 0}) && book.create(Action::Sleep, L"1", {100000, 0}),
          "prepare validation flow");
    for (const auto invalid : {L"", L"0", L"10081", L"-1", L"1.5", L"18446744073709551616"}) {
        check(!book.create(Action::Sleep, invalid, {100000, 0}) &&
              !book.edit(1, Action::Sleep, invalid, {100000, 0}) &&
              book.error() == BookError::InvalidInput && book.list().size() == 1 &&
              book.list()[0].occurrence_id == 1, "invalid duration preserves occurrence");
    }
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    check(!book.create(static_cast<Action>(99), L"1", {100000, 0}), "invalid action rejected");
    check(!book.edit(1, static_cast<Action>(99), L"1", {100000, 0}), "invalid edited action rejected");
    for (const auto time : {ClockSample{maximum, 0}, ClockSample{0, maximum}}) {
        check(!book.create(Action::Sleep, L"1", time) && !book.edit(1, Action::Sleep, L"1", time),
              "deadline overflow rejected");
    }
    check(book.create(Action::Shutdown, L"10080", {100000, 0}), "upper duration bound accepted");
    for (int i = 2; i < 1000; ++i)
        check(book.create(Action::Sleep, std::to_wstring(i + 1), {100000, 0}), "up to 1000 records accepted");
    check(!book.create(Action::Sleep, L"1", {100000, 0}) && book.list().size() == 1000 &&
          book.error() == BookError::InvalidInput, "capacity failure preserves all records");
    check(book.erase(500) && book.create(Action::Sleep, L"1001", {100000, 0}) &&
          book.list().back().id == 1001, "deleted IDs never reused");
    for (const bool schedule_id_exhausted : {false, true}) {
        MemoryStorage exhausted;
        exhausted.data = {LoadStatus::Loaded, {schedule_id_exhausted ? maximum : 2,
            maximum, {{1, 1, Action::Sleep, 1, 160000, false, OccurrenceState::Scheduled}}}};
        ScheduleBook limit(exhausted);
        check(limit.initialize({100000, 0}) && !limit.create(Action::Sleep, L"1", {100000, 0}) &&
              !limit.edit(1, Action::Sleep, L"1", {100000, 0}) && limit.list()[0].occurrence_id == 1,
              "identity exhaustion rejects rather than wrapping");
    }
}
void failures_in_processing_require_explicit_recovery() {
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(!book.create(Action::Sleep, L"1", {100000, 0}) &&
          book.error() == BookError::NotReady && book.tick({100000, 0}).empty(),
          "uninitialized book cannot mutate or deliver");
    check(book.initialize({100000, 0}) && book.create(Action::Sleep, L"1", {100000, 0}),
          "prepare processing failure");
    storage.fail_save = true;
    check(book.tick({160000, 60000}).empty() && !book.writable() &&
          book.error() == BookError::Storage && book.list()[0].state == OccurrenceState::Scheduled,
          "intent commit failure delivers nothing and leaves committed state visible");
    storage.fail_save = false;
    check(book.tick({160001, 60001}).empty() && !book.reconcile({160001, 60001}),
          "storage recovery alone does not restart processing");
    check(book.initialize({160001, 60001}) && book.list()[0].state == OccurrenceState::Expired &&
          book.tick({160001, 60001}).empty(), "explicit reload expires failed intent safely");
    check(book.edit(1, Action::Hibernate, L"1", {200000, 100000}), "prepare result failure");
    const auto deliveries = book.tick({260000, 160000});
    check(deliveries.size() == 1, "delivery available after durable intent");
    storage.fail_save = true;
    check(!book.complete(deliveries[0]) && !book.writable() &&
          book.list()[0].state == OccurrenceState::Uncertain, "result failure keeps uncertain intent");
    storage.fail_save = false;
    check(book.tick({260001, 160001}).empty(), "failed result never retries effect");
    ScheduleBook restarted(storage);
    check(restarted.initialize({300000, 0}) && restarted.tick({300000, 0}).empty() &&
          restarted.list()[0].state == OccurrenceState::Uncertain, "result failure survives process restart");
    check(restarted.edit(1, Action::Sleep, L"1", {300000, 0}), "prepare missed resume failure");
    storage.fail_save = true;
    check(!restarted.reconcile({360000, 60000}) && !restarted.writable() &&
          restarted.tick({360000, 60000}).empty(), "failed expiration blocks timer");
    ScheduleBook failed_start(storage);
    check(!failed_start.initialize({360000, 0}) && !failed_start.writable() &&
          failed_start.tick({360000, 0}).empty(), "failed startup expiration blocks timer");
    storage.fail_save = false;
    check(failed_start.initialize({360000, 0}) &&
          failed_start.list()[0].state == OccurrenceState::Expired, "startup can be explicitly recovered");
}
void noops_and_selection_are_safe() {
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(book.initialize({100000, 0}) && book.create(Action::Sleep, L"1", {100000, 0}) &&
          book.create(Action::Hibernate, L"2", {100000, 0}), "prepare selection flow");
    check(!book.edit(99, Action::Sleep, L"1", {100000, 0}) && book.error() == BookError::NotFound,
          "missing edit is explicit");
    check(!book.set_paused(99, true, {100000, 0}) && !book.erase(99) && !book.cancel(99) &&
          book.list().size() == 2, "missing commands preserve existing records");
    storage.fail_save = true;
    check(book.set_paused(1, false, {100000, 0}), "unchanged pause is successful without write");
    storage.fail_save = false;
    check(book.cancel(1) && book.remaining_ms(1, {100000, 0}) == 0, "cancelled has no remaining duration");
    storage.fail_save = true;
    check(book.cancel(1), "repeat cancellation is no-op");
    storage.fail_save = false;
    auto deliveries = book.tick({220000, 120000});
    check(deliveries.size() == 1 && deliveries[0].id == 2 && deliveries[0].action == Action::Hibernate,
          "only uncancelled selection is delivered");
    check(!book.complete({2, deliveries[0].occurrence_id + 1, Action::Hibernate}) &&
          !book.complete({2, deliveries[0].occurrence_id, Action::Sleep}), "stale or mismatched results rejected");
    check(book.complete(deliveries[0]), "matching completion accepted");
    storage.fail_save = true;
    check(book.complete(deliveries[0]), "duplicate completion does not require write");
    check(book.remaining_ms(2, {100000, 0}) == 0 && book.remaining_ms(99, {100000, 0}) == 0,
          "completed and absent records have no countdown");
}
void impossible_recovery_deadline_is_blocked() {
    MemoryStorage storage;
    storage.data = {LoadStatus::Loaded, {2, 2, {{1, 1, Action::Sleep, 1,
        std::numeric_limits<std::uint64_t>::max(), false, OccurrenceState::Scheduled}}}};
    ScheduleBook book(storage);
    check(!book.initialize({0, 1}) && !book.writable() && book.error() == BookError::InvalidData,
          "unrepresentable monotonic restoration is blocked without wrapping");
    check(!book.create(Action::Sleep, L"1", {0, 1}) && book.error() == BookError::InvalidData &&
          book.tick({0, 1}).empty(), "blocked commands preserve the diagnostic cause");
    ScheduleBook readable(storage);
    check(readable.initialize({1, 1}) && readable.remaining_ms(1, {1, 1}) ==
          std::numeric_limits<std::uint64_t>::max() - 1,
          "largest representable recovery deadline remains valid");
}
void paused_time_and_multiple_deliveries_survive_reopening() {
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(book.initialize({100000, 0}) && book.create(Action::Sleep, L"1", {100000, 0}) &&
          book.set_paused(1, true, {110000, 10000}), "prepare persisted pause");
    ScheduleBook restored(storage);
    check(restored.initialize({130000, 0}) && restored.list()[0].paused &&
          restored.remaining_ms(1, {130000, 0}) == 30000, "pause survives restart without freezing duration");
    check(restored.tick({160000, 30000}).empty() &&
          restored.list()[0].state == OccurrenceState::Expired, "paused timer expires without effect");
    check(restored.set_paused(1, false, {160000, 30000}) && restored.tick({160000, 30000}).empty(),
          "unpausing already expired record cannot revive it");
    check(restored.edit(1, Action::Sleep, L"1", {200000, 50000}) &&
          restored.create(Action::Hibernate, L"2", {200000, 50000}), "prepare independent deliveries");
    const auto first = restored.tick({260000, 110000});
    check(first.size() == 1 && first[0].id == 1 && restored.complete(first[0]),
          "earlier record delivered independently");
    ScheduleBook restarted(storage);
    check(restarted.initialize({270000, 10}) && restarted.list()[0].state == OccurrenceState::Completed &&
          restarted.remaining_ms(2, {270000, 10}) == 50000, "remaining future record restores independently");
    const auto second = restarted.tick({320000, 50010});
    check(second.size() == 1 && second[0].id == 2 && second[0].occurrence_id != first[0].occurrence_id &&
          restarted.tick({320000, 50010}).empty(), "distinct occurrence delivered only once");
}
void civil_clock_checkpoint_survives_restart() {
    for (const auto changed_utc : {0ULL, 300000ULL, 900000ULL}) {
        MemoryStorage storage;
        ScheduleBook book(storage);
        check(book.initialize({600000, 1000}) && book.create(Action::Sleep, L"2", {600000, 1000}),
              "prepare civil clock checkpoint");
        check(book.remaining_ms(1, {changed_utc, 31000}) == 90000,
              "civil change leaves current duration unchanged");
        check(book.reconcile({changed_utc, 31000}), "checkpoint civil clock change");
        ScheduleBook reopened(storage);
        check(reopened.initialize({changed_utc + 10000, 500}) &&
              reopened.remaining_ms(1, {changed_utc + 10000, 500}) == 80000,
              "restart keeps checkpointed duration and counts time closed");
    }
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(book.initialize({600000, 1000}) && book.create(Action::Sleep, L"2", {600000, 1000}) &&
          book.set_paused(1, true, {600000, 1000}), "prepare paused checkpoint");
    storage.fail_save = true;
    check(!book.reconcile({300000, 31000}) && !book.writable() && book.error() == BookError::Storage &&
          book.list()[0].due_utc_ms == 720000 && book.tick({800000, 200000}).empty(),
          "failed checkpoint preserves committed data and blocks effects");
    storage.fail_save = false;
    check(book.initialize({600000, 1000}) && book.reconcile({300000, 31000}),
          "explicit recovery can checkpoint paused duration");
    ScheduleBook reopened(storage);
    check(reopened.initialize({310000, 500}) && reopened.list()[0].paused &&
          reopened.remaining_ms(1, {310000, 500}) == 80000,
          "paused duration preserves elapsed time across checkpoint restart");
    MemoryStorage at_epoch;
    at_epoch.data = {LoadStatus::Loaded, {2, 2, {{1, 1, Action::Sleep, 1, 0, false,
        OccurrenceState::Scheduled}}}};
    ScheduleBook expired(at_epoch);
    check(expired.initialize({0, 0}) && expired.list()[0].state == OccurrenceState::Expired,
          "deadline at civil epoch is expired data rather than malformed data");
    MemoryStorage unchanged;
    ScheduleBook stable(unchanged);
    check(stable.initialize({600000, 0}) && stable.create(Action::Sleep, L"1", {600000, 0}),
          "prepare unchanged civil anchor");
    unchanged.fail_save = true;
    check(stable.reconcile({630000, 30000}) && stable.writable(), "unchanged anchor requires no write");
    unchanged.fail_save = false;
    const auto intent = stable.tick({660000, 60000});
    check(intent.size() == 1, "prepare terminal checkpoint");
    unchanged.fail_save = true;
    check(stable.reconcile({100, 70000}) && stable.list()[0].due_utc_ms == 660000 &&
          stable.list()[0].state == OccurrenceState::Uncertain,
          "checkpoint never rewrites uncertain intent or requires a write for terminal state");
    check(!reopened.reconcile({std::numeric_limits<std::uint64_t>::max(), 500}) && !reopened.writable() &&
          reopened.error() == BookError::InvalidData && reopened.list()[0].due_utc_ms == 390000,
          "unrepresentable civil anchor fails closed without altering saved deadline");
}
void dispatch_tolerance() {
    for (const auto delay : {5000ULL, 5001ULL}) {
        MemoryStorage storage;
        ScheduleBook book(storage);
        check(book.initialize({0, 0}) && book.create(Action::Sleep, L"1", {0, 0}), "prepare dispatch tolerance");
        check(book.tick({60000 + delay, 60000 + delay}).size() == (delay == 5000 ? 1U : 0U),
              "normal dispatch tolerates exactly five seconds, never more");
    }
}
void coincident_commands_preserve_previous_schedule() {
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage, calendar);
    check(book.initialize({0,0}) && book.create(Action::Sleep,L"1",{0,0}), "prepare conflict command");
    const auto previous = storage.data.snapshot;
    check(!book.create_once(Action::Shutdown,{1970,1,1,0,1},{0,0}) &&
          book.error() == BookError::Conflict && book.conflicting_id() == 1 &&
          book.list().size() == 1 && storage.data.snapshot.next_id == previous.next_id &&
          storage.data.snapshot.next_occurrence_id == previous.next_occurrence_id,
          "same UTC rejects other action without consuming identities or replacing storage");
    check(book.create(Action::Hibernate,L"2",{0,0}) && book.list().back().id == 2,
          "successful command clears conflict and uses unconsumed identity");
    check(!book.edit(2,Action::Sleep,L"1",{0,0}) && book.conflicting_id() == 1 &&
          book.list()[1].minutes == 2 && book.list()[1].occurrence_id == 2,
          "conflicting edit preserves previous configuration");
}
void weekly_conflicts_include_future_days_and_exceptions() {
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage,calendar);
    check(book.initialize({0,0}) && book.create_weekly(Action::Sleep,16,1,{0,0}), "Thursday weekly at epoch");
    check(!book.create_weekly(Action::Shutdown,24,1,{0,0}) && book.error() == BookError::Conflict &&
          book.conflicting_id() == 1, "overlap on Thursday is conflict even with earlier Wednesday occurrence next week");
    check(book.create_weekly(Action::Shutdown,8,1,{0,0}), "distinct weekdays do not conflict");
    check(!book.create_once(Action::Hibernate,{1970,1,8,0,1},{0,0}) && book.conflicting_id() == 1,
          "finite occurrence conflicts with future weekly date beyond its next due");
    check(book.cancel(1) && book.create_once(Action::Sleep,{1970,1,1,0,1},{0,0}),
          "cancelled weekly occurrence leaves its consumed civil date available");
    check(book.set_paused(2,true,{0,0}) && book.create_weekly(Action::Hibernate,8,1,{0,0}),
          "paused recurrence does not reserve future days");
    check(!book.set_paused(2,false,{0,0}) && book.error() == BookError::Conflict &&
          book.conflicting_id() == 4 && book.list()[1].paused,
          "reactivation rejects future overlap while preserving pause");
    check(book.edit_weekly(2,Action::Shutdown,8,2,{0,0}) && book.list()[1].paused &&
          book.set_paused(2,false,{0,0}), "paused edit remains paused and adjusted reactivation succeeds");
}
void recovered_collisions_suspend_every_member() {
    MemoryStorage storage;
    storage.data = {LoadStatus::Loaded, {5,5,{
        {1,1,Action::Sleep,1,60000,false,OccurrenceState::Scheduled},
        {2,2,Action::Shutdown,1,60000,false,OccurrenceState::Scheduled},
        {3,3,Action::Hibernate,1,60000,false,OccurrenceState::Scheduled},
        {4,4,Action::Sleep,1,60001,false,OccurrenceState::Scheduled}}}};
    ScheduleBook book(storage);
    check(book.initialize({0,0}) && book.error() == BookError::None &&
          book.list()[0].conflict_with == 2 && book.list()[1].conflict_with == 1 &&
          book.list()[2].conflict_with == 1 && !book.list()[3].conflict_with &&
          !book.list()[0].paused && storage.data.snapshot.records[2].conflict_with == 1,
          "legacy three-way duplicates suspend every member durably without choosing winner or pausing");
    const auto deliveries = book.tick({60001,60001});
    check(deliveries.size() == 1 && deliveries[0].id == 4 && book.list()[0].conflict_with == 2,
          "all conflicting actions blocked before batch delivery while one millisecond difference is independent");
    storage.fail_save = true;
    check(!book.erase(1) && book.list().size() == 4 && book.list()[1].conflict_with == 1,
          "failed removal preserves component and saved references");
    storage.fail_save = false;
    check(book.erase(1) && book.list()[0].conflict_with == 3 && book.list()[1].conflict_with == 2,
          "removing first member recalculates remaining three-way links");
    check(book.cancel(2) && !book.list()[0].conflict_with && !book.list()[1].conflict_with,
          "cancelling releases both finite reservations transactionally");
}
void timezone_collision_blocks_before_any_delivery() {
    for (const bool reconcile : {false,true}) {
        MemoryStorage storage;
        TestCalendar calendar;
        ScheduleBook book(storage,calendar);
        check(book.initialize({0,0}) && book.create_once(Action::Sleep,{1970,1,1,1,0},{0,0}) &&
              book.create_weekly(Action::Shutdown,127,120,{0,0}) &&
              book.create_once(Action::Hibernate,{1970,1,1,0,30},{0,0}), "prepare timezone collision and independent due");
        calendar.offset_minutes = 60;
        if (reconcile) check(book.reconcile({1800000,1800000}), "explicit timezone event reconciles");
        const auto effects = book.tick({1800000,1800000});
        check(book.list()[0].conflict_with == 2 && book.list()[1].conflict_with == 1 &&
              !book.list()[0].paused && !book.list()[1].paused &&
              (reconcile ? effects.empty() : effects.size() == 1 && effects[0].id == 3),
              "timezone collision is found without host notification before independent effects");
        check(book.tick({3600000,3600000}).empty(), "neither action wins at common deadline");
    }
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage,calendar);
    check(book.initialize({0,0}) && book.create_once(Action::Sleep,{1970,1,1,1,0},{0,0}) &&
          book.create_weekly(Action::Shutdown,127,120,{0,0}) &&
          book.create(Action::Hibernate,L"30",{0,0}), "prepare suspension persistence failure");
    calendar.offset_minutes = 60;
    storage.fail_save = true;
    check(book.tick({1800000,1800000}).empty() && !book.writable() && book.error() == BookError::Storage &&
          !book.list()[0].conflict_with && book.list()[2].state == OccurrenceState::Scheduled,
          "failed conflict persistence blocks the entire batch and preserves all prior records");
}
void collision_at_timezone_changed_deadline_never_selects_winner() {
    MemoryStorage storage;
    TestCalendar calendar;
    ScheduleBook book(storage,calendar);
    check(book.initialize({0,0}) && book.create_once(Action::Sleep,{1970,1,1,1,0},{0,0}) &&
          book.create_weekly(Action::Shutdown,127,120,{0,0}), "prepare timezone change at due instant");
    calendar.offset_minutes = 60;
    check(book.tick({3600000,3600000}).empty() && book.list()[0].conflict_with == 2 &&
          book.list()[1].conflict_with == 1,
          "timezone change at exact due marks both before weekly expiration could leave a winner");
}
void forced_shutdown_is_orthogonal_to_conflicts_and_preserved_on_edit() {
    MemoryStorage storage;
    storage.data = {LoadStatus::Loaded,{2,2,{{1,1,Action::Shutdown,1,60000,false,OccurrenceState::Scheduled}}}};
    storage.data.snapshot.records[0].force_shutdown = true;
    TestCalendar calendar;
    ScheduleBook book(storage,calendar);
    check(book.initialize({0,0}) && !book.create(Action::Shutdown,L"1",{0,0}) &&
          book.error() == BookError::Conflict, "normal and forced shutdown reserve the same UTC instant");
    check(book.edit_once(1,Action::Shutdown,{1970,1,1,0,2},{0,0}) && book.list()[0].force_shutdown &&
          book.edit_weekly(1,Action::Shutdown,127,3,{0,0}) && book.list()[0].force_shutdown &&
          book.edit(1,Action::Shutdown,L"4",{0,0}) && book.list()[0].force_shutdown,
          "editing a shutdown retains explicit existing force preference across modes");
    check(book.edit_weekly(1,Action::Sleep,127,5,{0,0}) && !book.list()[0].force_shutdown &&
          book.edit_once(1,Action::Shutdown,{1970,1,1,0,6},{0,0}) && !book.list()[0].force_shutdown,
          "changing to sleep clears force and returning to shutdown never silently reenables it");
}
void monotonic_projection_has_no_stale_or_epoch_false_conflicts() {
    {
        MemoryStorage storage;
        TestCalendar calendar;
        ScheduleBook book(storage,calendar);
        check(book.initialize({0,0}) && book.create(Action::Sleep,L"1",{0,0}) &&
              book.create_once(Action::Hibernate,{1970,1,1,0,1},{30000,0}),
              "manual clock change projects duration before candidate validation instead of stale saved UTC");
        check(book.tick({60000,30000}).size() == 1 && !book.list()[0].conflict_with,
              "different monotonic projected instant does not block fixed UTC delivery");
    }
    {
        MemoryStorage storage;
        TestCalendar calendar;
        ScheduleBook book(storage,calendar);
        check(book.initialize({0,0}) && book.create(Action::Sleep,L"1",{0,0}) &&
              book.create_once(Action::Hibernate,{1970,1,1,0,2},{0,0}), "prepare projected collision");
        check(book.tick({60000,0}).empty() && book.list()[0].conflict_with == 2 &&
              storage.data.snapshot.records[0].due_utc_ms == 120000,
              "timer saves projected duration anchor with collision so recovery uses the same instant");
        ScheduleBook reopened(storage,calendar);
        check(reopened.initialize({60001,0}) && reopened.list()[0].conflict_with == 2 &&
              reopened.remaining_ms(1,{60001,0}) == 59999,
              "collision and monotonic remaining time agree after recovery");
    }
    {
        MemoryStorage storage;
        ScheduleBook book(storage);
        check(book.initialize({0,0}) && book.create(Action::Sleep,L"1",{0,0}) &&
              book.create(Action::Hibernate,L"2",{0,0}), "prepare two pre-epoch projections");
        const auto effects = book.tick({0,120001});
        check(effects.size() == 1 && effects[0].id == 2 && !book.list()[0].conflict_with &&
              !book.list()[1].conflict_with && book.list()[0].state == OccurrenceState::Expired,
              "distinct elapsed deadlines before UTC epoch cannot become a fabricated collision at zero");
    }
}
struct RejectingBackend : PowerBackend {
    int requests = 0;
    PowerCapabilities query() override { return {}; }
    ExecResult request(Action) override { ++requests; return {ExecStatus::Rejected, Availability::Available, 5}; }
};
void failed_power_request_is_not_repeated_after_restart() {
    MemoryStorage storage;
    ScheduleBook book(storage);
    check(book.initialize({0, 0}) && book.create(Action::Hibernate, L"1", {0, 0}), "prepare failing request");
    const auto deliveries = book.tick({60000, 60000});
    check(deliveries.size() == 1 && book.list()[0].state == OccurrenceState::Uncertain, "intent is durable before the request");
    RejectingBackend backend;
    check(execute_power_action(backend, deliveries[0].action).status == ExecStatus::Rejected, "backend failure reported");
    // Host crashes here: complete() is never called.
    ScheduleBook restarted(storage);
    check(restarted.initialize({61000, 1000}) && restarted.list()[0].state != OccurrenceState::Scheduled &&
          restarted.list()[0].state != OccurrenceState::Completed && restarted.tick({62000, 2000}).empty() &&
          backend.requests == 1, "failed request never repeats after restart and is not shown as completed");
}
int main() {
    failed_power_request_is_not_repeated_after_restart();
    monotonic_projection_has_no_stale_or_epoch_false_conflicts();
    forced_shutdown_is_orthogonal_to_conflicts_and_preserved_on_edit();
    collision_at_timezone_changed_deadline_never_selects_winner();
    recovered_collisions_suspend_every_member();
    timezone_collision_blocks_before_any_delivery();
    weekly_conflicts_include_future_days_and_exceptions();
    coincident_commands_preserve_previous_schedule();
    calendar_validation_and_limit_failures();
    editing_modes_and_recovery_tolerance();
    recurring_calendar_boundaries();
    gaps_and_folds_are_consumed_only_once();
    weekly_clock_changes_and_pauses();
    recurring_failures_are_transactional();
    calendar_failures_block_effects();
    weekly_keeps_rule_after_delivery_and_cancel();
    postpone_moves_only_the_current_occurrence();
    once_uses_fixed_instant();
    dispatch_tolerance();
    create_and_restore();
    manage_list_transactionally();
    recover_and_deliver_once();
    damaged_storage_blocks_processing();
    reject_input_and_capacity_without_loss();
    failures_in_processing_require_explicit_recovery();
    noops_and_selection_are_safe();
    impossible_recovery_deadline_is_blocked();
    paused_time_and_multiple_deliveries_survive_reopening();
    civil_clock_checkpoint_survives_restart();
    std::cout << checks << " schedule book checks passed\n";
}
