#include "../src/windows/schedule_file.hpp"
#include "../src/schedule_book.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iterator>
using namespace power_timer;
static unsigned checks = 0;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
 std::cerr << "FAIL line " << __LINE__ << ": " << #expression << '\n'; std::exit(EXIT_FAILURE); } } while (false)
int main(int argc, char** argv) {
    CHECK(argc == 2);
    const std::filesystem::path fixture(argv[1]);
    std::filesystem::create_directories(fixture);
    const auto file = fixture / "roundtrip.pt";
    std::filesystem::remove(file);
    FileScheduleStorage storage(file.wstring());
    CHECK(storage.load().status == LoadStatus::Missing);
    ScheduleSnapshot expected{2, 2, {{1, 1, Action::Sleep, 15, 901000, false, OccurrenceState::Scheduled}}};
    CHECK(storage.save(expected));
    const auto actual = storage.load();
    CHECK(actual.status == LoadStatus::Loaded);
    CHECK(actual.snapshot.next_id == 2);
    CHECK(actual.snapshot.records.size() == 1);
    CHECK(actual.snapshot.records.front().due_utc_ms == 901000);
    CHECK(actual.snapshot.records.front().action == Action::Sleep);
    CHECK(encode_snapshot(expected) == "POWER_TIMER 5\n2 2 1 0\n0\n0\n1 1 1 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n");
    {
        std::ifstream in(file, std::ios::binary);
        const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(saved.starts_with("POWER_TIMER 5\n"));
    }
    // A version-1 duration loads with the new fields at their neutral values.
    const std::string legacy = "POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 0 0\n";
    const auto migrated = decode_snapshot(legacy);
    CHECK(migrated.has_value());
    CHECK(migrated->records.front().kind == ScheduleKind::Duration);
    CHECK(migrated->records.front().civil == CivilTime{});
    CHECK(migrated->records.front().weekdays == 0);
    CHECK(migrated->records.front().minute_of_day == 0);
    CHECK(migrated->records.front().civil_high_water == 0);
    CHECK(migrated->records.front().last_occurrence_id == 0);
    CHECK(migrated->records.front().last_state == OccurrenceState::Scheduled);
    CHECK(migrated->records.front().conflict_with == 0);
    CHECK(!migrated->records.front().force_shutdown);
    const auto migrated_data = encode_snapshot(*migrated);
    CHECK(migrated_data.starts_with("POWER_TIMER 5\n"));
    const auto migrated_again = decode_snapshot(migrated_data);
    CHECK(migrated_again.has_value());
    CHECK(migrated_again->records.front().minutes == 15);
    const auto legacy_file = fixture / "legacy.pt";
    {
        std::ofstream out(legacy_file, std::ios::binary | std::ios::trunc);
        out << legacy;
    }
    FileScheduleStorage legacy_storage(legacy_file.wstring());
    const auto from_legacy_file = legacy_storage.load();
    CHECK(from_legacy_file.status == LoadStatus::Loaded);
    ScheduleBook legacy_book(legacy_storage);
    CHECK(legacy_book.initialize({1000, 1000}));
    CHECK(legacy_book.list().front().kind == ScheduleKind::Duration);
    HANDLE legacy_lock = CreateFileW(legacy_file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(legacy_lock != INVALID_HANDLE_VALUE);
    CHECK(!legacy_book.set_paused(1, true, {1000, 1000}));
    CHECK(!legacy_book.list().front().paused);
    {
        std::ifstream in(legacy_file, std::ios::binary);
        const std::string preserved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(preserved == legacy);
    }
    CHECK(CloseHandle(legacy_lock));
    CHECK(legacy_book.set_paused(1, true, {1000, 1000}));
    {
        std::ifstream in(legacy_file, std::ios::binary);
        const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(saved.starts_with("POWER_TIMER 5\n"));
    }
    CHECK(legacy_storage.load().status == LoadStatus::Loaded);
    CHECK(legacy_storage.load().snapshot.records.front().paused);
    // Civil rules and the last result remain available after replacing the file.
    ScheduleSnapshot civil_rules{3, 9, {}};
    ScheduleRecord once{1, 1, Action::Sleep, 0, 1791505800000, false,
        OccurrenceState::Scheduled};
    once.kind = ScheduleKind::Once;
    once.civil = {2026, 10, 8, 21, 30};
    ScheduleRecord weekly{2, 8, Action::Hibernate, 0, 1791592200000, true,
        OccurrenceState::Scheduled};
    weekly.kind = ScheduleKind::Weekly;
    weekly.civil = {2026, 10, 9, 21, 30};
    weekly.weekdays = 127;
    weekly.minute_of_day = 1290;
    weekly.civil_high_water = 202610082130;
    weekly.last_occurrence_id = 7;
    weekly.last_state = OccurrenceState::Expired;
    civil_rules.records = {once, weekly};
    const auto civil_file = fixture / "civil.pt";
    std::filesystem::remove(civil_file);
    FileScheduleStorage civil_storage(civil_file.wstring());
    CHECK(civil_storage.save(civil_rules));
    const auto civil_loaded = FileScheduleStorage(civil_file.wstring()).load();
    CHECK(civil_loaded.status == LoadStatus::Loaded);
    CHECK(civil_loaded.snapshot.records.size() == 2);
    CHECK(civil_loaded.snapshot.records[0].kind == ScheduleKind::Once);
    CHECK((civil_loaded.snapshot.records[0].civil == CivilTime{2026, 10, 8, 21, 30}));
    CHECK(civil_loaded.snapshot.records[1].kind == ScheduleKind::Weekly);
    CHECK((civil_loaded.snapshot.records[1].civil == CivilTime{2026, 10, 9, 21, 30}));
    CHECK(civil_loaded.snapshot.records[1].weekdays == 127);
    CHECK(civil_loaded.snapshot.records[1].minute_of_day == 1290);
    CHECK(civil_loaded.snapshot.records[1].civil_high_water == 202610082130);
    CHECK(civil_loaded.snapshot.records[1].last_occurrence_id == 7);
    CHECK(civil_loaded.snapshot.records[1].last_state == OccurrenceState::Expired);
    // A reconciled civil deadline describes remaining time, not the original duration.
    const auto rebased_file = fixture / "rebased.pt";
    std::filesystem::remove(rebased_file);
    FileScheduleStorage rebased_storage(rebased_file.wstring());
    ScheduleSnapshot rebased{2, 2, {{1, 1, Action::Sleep, 15, 30000, false, OccurrenceState::Scheduled}}};
    CHECK(rebased_storage.save(rebased));
    const auto rebased_roundtrip = rebased_storage.load();
    CHECK(rebased_roundtrip.status == LoadStatus::Loaded);
    CHECK(rebased_roundtrip.snapshot.records.front().minutes == 15);
    CHECK(rebased_roundtrip.snapshot.records.front().due_utc_ms == 30000);
    rebased.records.front().due_utc_ms = 0;
    CHECK(rebased_storage.save(rebased));
    const auto zero_deadline = rebased_storage.load();
    CHECK(zero_deadline.status == LoadStatus::Loaded);
    CHECK(zero_deadline.snapshot.records.front().due_utc_ms == 0);
    CHECK(!decode_snapshot("POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 2 0\n"));
    const std::string canonical_v2 =
        "POWER_TIMER 2\n2 2 1\n1 1 1 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0\n";
    CHECK(decode_snapshot(canonical_v2).has_value());
    const std::string once_v2 =
        "POWER_TIMER 2\n2 2 1\n1 1 1 0 1791505800000 0 0 1 2026 10 8 21 30 0 0 0 0 0\n";
    const std::string weekly_v2 =
        "POWER_TIMER 2\n3 9 1\n2 8 2 0 1791592200000 1 0 2 2026 10 9 21 30 127 1290 202610082130 7 2\n";
    CHECK(decode_snapshot(once_v2).has_value());
    CHECK(decode_snapshot(weekly_v2).has_value());
    for (const auto& old_bytes : {legacy, canonical_v2}) {
        const auto migration_file = fixture / (old_bytes == legacy ? "migrate-v1.pt" : "migrate-v2.pt");
        {
            std::ofstream out(migration_file, std::ios::binary | std::ios::trunc);
            out << old_bytes;
        }
        FileScheduleStorage migration_storage(migration_file.wstring());
        auto loaded = migration_storage.load();
        CHECK(loaded.status == LoadStatus::Loaded);
        CHECK(loaded.snapshot.records.front().conflict_with == 0);
        CHECK(!loaded.snapshot.records.front().force_shutdown);
        loaded.snapshot.records.front().paused = true;
        HANDLE migration_lock = CreateFileW(migration_file.c_str(), GENERIC_READ, FILE_SHARE_READ,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        CHECK(migration_lock != INVALID_HANDLE_VALUE);
        CHECK(!migration_storage.save(loaded.snapshot));
        CHECK(CloseHandle(migration_lock));
        {
            std::ifstream in(migration_file, std::ios::binary);
            const std::string preserved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            CHECK(preserved == old_bytes);
        }
        CHECK(migration_storage.save(loaded.snapshot));
        const auto migrated_file = migration_storage.load();
        CHECK(migrated_file.status == LoadStatus::Loaded);
        CHECK(migrated_file.snapshot.records.front().paused);
        CHECK(migrated_file.snapshot.records.front().conflict_with == 0);
        CHECK(!migrated_file.snapshot.records.front().force_shutdown);
        {
            std::ifstream in(migration_file, std::ios::binary);
            const std::string saved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            CHECK(saved.starts_with("POWER_TIMER 5\n"));
        }
    }
    auto forced = expected;
    forced.records.front().action = Action::Shutdown;
    forced.records.front().force_shutdown = true;
    const auto force_file = fixture / "force.pt";
    FileScheduleStorage force_storage(force_file.wstring());
    CHECK(force_storage.save(forced));
    CHECK(force_storage.load().status == LoadStatus::Loaded);
    CHECK(force_storage.load().snapshot.records.front().force_shutdown);
    ScheduleSnapshot conflicts{4, 4, {
        {1, 1, Action::Shutdown, 15, 901000, false, OccurrenceState::Scheduled},
        {2, 2, Action::Sleep, 15, 901000, false, OccurrenceState::Scheduled},
        {3, 3, Action::Hibernate, 15, 961000, true, OccurrenceState::Scheduled}
    }};
    conflicts.records[0].conflict_with = 2;
    conflicts.records[0].force_shutdown = true;
    conflicts.records[1].conflict_with = 1;
    const auto conflict_file = fixture / "conflicts.pt";
    FileScheduleStorage conflict_storage(conflict_file.wstring());
    CHECK(conflict_storage.save(conflicts));
    const auto restored_conflicts = FileScheduleStorage(conflict_file.wstring()).load();
    CHECK(restored_conflicts.status == LoadStatus::Loaded);
    CHECK(restored_conflicts.snapshot.next_id == 4);
    CHECK(restored_conflicts.snapshot.next_occurrence_id == 4);
    CHECK(restored_conflicts.snapshot.records.size() == 3);
    CHECK(restored_conflicts.snapshot.records[0].id == 1);
    CHECK(restored_conflicts.snapshot.records[0].conflict_with == 2);
    CHECK(restored_conflicts.snapshot.records[0].force_shutdown);
    CHECK(restored_conflicts.snapshot.records[1].id == 2);
    CHECK(restored_conflicts.snapshot.records[1].conflict_with == 1);
    CHECK(!restored_conflicts.snapshot.records[1].force_shutdown);
    CHECK(restored_conflicts.snapshot.records[2].id == 3);
    CHECK(restored_conflicts.snapshot.records[2].paused);
    CHECK(restored_conflicts.snapshot.records[2].conflict_with == 0);
    for (const auto invalid_force : {
        "2", "01", "18446744073709551616", "-1", "1 0", "1\r"
    }) {
        CHECK(!decode_snapshot(
            "POWER_TIMER 3\n2 2 1\n1 1 0 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 "
            + std::string(invalid_force) + "\n"));
    }
    CHECK(!decode_snapshot(
        "POWER_TIMER 3\n2 2 1\n1 1 1 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 1\n"));
    const std::string v3_prefix =
        "POWER_TIMER 3\n2 2 1\n1 1 0 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 ";
    for (const auto invalid_suffix : {
        "1 0\n",                    // a conflict cannot refer to the same ID
        "2 0\n",                    // a conflict cannot refer beyond next_id
        "01 0\n",                   // noncanonical conflict ID
        "18446744073709551616 0\n", // overflowing conflict ID
        "-1 0\n",                   // signed conflict ID
        "0\n",                     // missing force flag
        "0 0 0\n"                 // extra field
    }) CHECK(!decode_snapshot(v3_prefix + invalid_suffix));
    CHECK(!decode_snapshot(
        "POWER_TIMER 3\n2 2 1\n1 1 3 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"));
    CHECK(decode_snapshot("POWER_TIMER 3\n2 2 0\n"));
    // Version 4 files (no postponement field) remain readable.
    CHECK(decode_snapshot("POWER_TIMER 4\n2 2 1 0\n0\n0\n1 1 1 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"));
    CHECK(decode_snapshot("POWER_TIMER 4\n2 2 1 0\n1 1 1 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n")); // early v4: no theme/locale lines
    CHECK(decode_snapshot("POWER_TIMER 4\n1 1 0 0\n")); // early v4, empty
    CHECK(decode_snapshot("POWER_TIMER 5\n2 2 1 0\n0\n0\n1 1 1 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"));
    // A postponement is only valid on a weekly rule.
    CHECK(!decode_snapshot("POWER_TIMER 5\n2 2 1 0\n0\n0\n1 1 1 15 901000 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 900000\n"));
    CHECK(!decode_snapshot("POWER_TIMER 2\n2 2 1\n1 1 1 15 901000 0 0\n"));
    CHECK(!decode_snapshot(canonical_v2 + "tail"));
    const std::string v2_record_prefix = "POWER_TIMER 2\n2 2 1\n1 1 1 15 901000 0 0 ";
    for (const auto invalid_tail : {
        "3 0 0 0 0 0 0 0 0 0 0\n",               // unknown kind
        "4294967297 0 0 0 0 0 0 0 0 0 0\n",      // kind exceeds enum width
        "0 2147483648 0 0 0 0 0 0 0 0 0\n",     // year exceeds int width
        "0 0 4294967296 0 0 0 0 0 0 0 0\n",     // month exceeds unsigned width
        "0 0 0 0 0 0 4294967296 0 0 0 0\n",     // weekdays exceeds unsigned width
        "0 0 0 0 0 0 0 4294967296 0 0 0\n",     // minute exceeds unsigned width
        "0 0 0 0 0 0 0 0 0 0 5\n",              // unknown last state
        "0 0 0 0 0 0 0 0 0 0 00\n",             // noncanonical last state
        "0 0 0 0 0 0 0 0 0 0 0 0\n",            // unknown extra field
        "0 0 0 0 0 0 0 0 0 0 0\r\n",          // CRLF
        "0 0 0 0 0 0 0 0 0 0 0"                 // truncation
    }) CHECK(!decode_snapshot(v2_record_prefix + invalid_tail));
    const std::string weekly_prefix = "POWER_TIMER 2\n3 9 1\n2 8 2 0 1791592200000 1 0 2 ";
    for (const auto invalid_rule : {
        "2026 2 30 21 30 127 1290 0 7 2\n",                 // nonexistent date
        "2026 10 9 21 30 0 1290 202610082130 7 2\n",       // no selected weekdays
        "2026 10 9 21 30 127 1289 202610082130 7 2\n",    // time and rule differ
        "2026 10 9 21 30 127 1290 202610092130 7 2\n",    // consumed current civil time
        "2026 10 9 21 30 127 1290 202610082130 8 2\n",    // last ID aliases current
        "2026 10 9 21 30 127 1290 202610082130 7 0\n"     // last result cannot be pending
    }) CHECK(!decode_snapshot(weekly_prefix + invalid_rule));
    CHECK(!decode_snapshot(
        "POWER_TIMER 2\n2 2 1\n1 1 1 0 1791505800000 0 0 1 2026 10 8 21 30 1 0 0 0 0\n"));
    CHECK(!decode_snapshot(
        "POWER_TIMER 2\n2 2 1\n1 1 1 15 901000 0 0 0 2026 10 8 21 30 0 0 0 0 0\n"));
    // Corrupt values cannot become runnable records or alias another identity.
    for (const auto invalid : {
        "POWER_TIMER 2\n2 2 1\n1 1 1 15 901000 0 0\n",
        "POWER_TIMER 1\n0 2 0\n",
        "POWER_TIMER 1\n2 0 0\n",
        "POWER_TIMER 1\n2 2 1\n0 1 1 15 901000 0 0\n",
        "POWER_TIMER 1\n2 2 1\n1 0 1 15 901000 0 0\n",
        "POWER_TIMER 1\n2 2 1\n2 1 1 15 901000 0 0\n",
        "POWER_TIMER 1\n2 2 1\n1 2 1 15 901000 0 0\n",
        "POWER_TIMER 1\n2 2 1\n1 1 3 15 901000 0 0\n",
        "POWER_TIMER 1\n2 2 1\n1 1 4294967297 15 901000 0 0\n",
        "POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 0 5\n",
        "POWER_TIMER 1\n2 2 1\n1 1 1 0 901000 0 0\n",
        "POWER_TIMER 1\n2 2 1\n1 1 1 10081 999999999 0 0\n",
        "POWER_TIMER 1\n3 3 2\n1 1 1 15 901000 0 0\n1 2 0 1 60000 0 0\n",
        "POWER_TIMER 1\n3 3 2\n1 1 1 15 901000 0 0\n2 1 0 1 60000 0 0\n",
        "POWER_TIMER 1\n2 2 1001\n",
        "POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 0 0\ntail",
        "POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 0 0",
        "POWER_TIMER 1\n18446744073709551616 2 0\n",
        "POWER_TIMER 1\n+2 2 0\n",
        "POWER_TIMER 1\n2 2 1\n1 1 -1 15 901000 0 0\n",
        "POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 0 0\n\n",
        "POWER_TIMER 1\n2\t2 0\n",
        "POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 0 0\r\n",
        "POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 0 0 0\n",
        "POWER_TIMER 1\n2 2 1\n1 1 1 18446744073709551615 901000 0 0\n"
    }) CHECK(!decode_snapshot(invalid));
    CHECK(!decode_snapshot(std::string(1024 * 1024 + 1, 'x')));
    auto bad_snapshot = expected;
    bad_snapshot.next_id = 1;
    CHECK(encode_snapshot(bad_snapshot).empty());
    CHECK(!storage.save(bad_snapshot));
    CHECK(storage.load().snapshot.records.front().due_utc_ms == 901000);
    CHECK(!decode_snapshot("POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 01 0\n"));
    CHECK(!decode_snapshot("POWER_TIMER 1\n02 2 0\n"));
    // Windows rejects replacement while another reader denies delete sharing.
    // The command is not reported saved, and a fresh storage instance sees the original.
    HANDLE lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(lock != INVALID_HANDLE_VALUE);
    auto changed = expected;
    changed.records.front().due_utc_ms = 999000;
    CHECK(!storage.save(changed));
    CHECK(CloseHandle(lock));
    FileScheduleStorage reopened(file.wstring());
    CHECK(reopened.load().status == LoadStatus::Loaded);
    CHECK(reopened.load().snapshot.records.front().due_utc_ms == 901000);
    CHECK(storage.save(changed));
    CHECK(reopened.load().snapshot.records.front().due_utc_ms == 999000);

    // A missing data file is distinct from an inaccessible directory/path.
    const auto missing_parent = fixture / "parent-does-not-exist" / "schedules.pt";
    FileScheduleStorage unavailable(missing_parent.wstring());
    CHECK(unavailable.load().status == LoadStatus::Unavailable);
    CHECK(!unavailable.save(expected));
    FileScheduleStorage directory(fixture.wstring());
    CHECK(directory.load().status == LoadStatus::Unavailable);
    CHECK(!directory.save(expected));

    // Malformed bytes are reported rather than partially recovered.
    const auto corrupt_file = fixture / "corrupt.pt";
    FileScheduleStorage corrupt(corrupt_file.wstring());
    const std::string canonical = "POWER_TIMER 1\n2 2 1\n1 1 1 15 901000 0 0\n";
    for (std::size_t length = 0; length < canonical.size(); ++length) {
        std::ofstream out(corrupt_file, std::ios::binary | std::ios::trunc);
        out.write(canonical.data(), static_cast<std::streamsize>(length));
        out.close();
        CHECK(corrupt.load().status == LoadStatus::Invalid);
    }
    for (std::size_t length = 0; length < canonical_v2.size(); ++length) {
        std::ofstream out(corrupt_file, std::ios::binary | std::ios::trunc);
        out.write(canonical_v2.data(), static_cast<std::streamsize>(length));
        out.close();
        CHECK(corrupt.load().status == LoadStatus::Invalid);
    }
    {
        std::ofstream out(corrupt_file, std::ios::binary | std::ios::trunc);
        const std::string oversized(1024 * 1024 + 1, 'x');
        out.write(oversized.data(), static_cast<std::streamsize>(oversized.size()));
    }
    CHECK(corrupt.load().status == LoadStatus::Invalid);
    {
        std::ofstream out(corrupt_file, std::ios::binary | std::ios::trunc);
        out << canonical;
    }
    CHECK(corrupt.load().status == LoadStatus::Loaded);
    CHECK(corrupt.load().snapshot.records.front().due_utc_ms == 901000);

    // Every action, terminal state and pause flag survives repeated replacement at the limit.
    const auto stress_file = fixture / "stress.pt";
    std::filesystem::remove(stress_file);
    FileScheduleStorage stress(stress_file.wstring());
    ScheduleSnapshot maximum{1001, 1001, {}};
    for (std::uint64_t id = 1; id <= 1000; ++id) {
        maximum.records.push_back({id, id, static_cast<Action>((id - 1) % 3),
            id == 1000 ? 10080u : 1u, 604900000 + id, id % 2 == 0,
            static_cast<OccurrenceState>((id - 1) % 5)});
    }
    for (unsigned cycle = 0; cycle < 32; ++cycle) {
        maximum.records.front().due_utc_ms = 604900001 + cycle;
        CHECK(stress.save(maximum));
        const auto restored = FileScheduleStorage(stress_file.wstring()).load();
        CHECK(restored.status == LoadStatus::Loaded);
        CHECK(restored.snapshot.next_id == 1001);
        CHECK(restored.snapshot.next_occurrence_id == 1001);
        CHECK(restored.snapshot.records.size() == 1000);
        CHECK(restored.snapshot.records.front().due_utc_ms == 604900001 + cycle);
        for (std::size_t i = 0; i < maximum.records.size(); ++i) {
            const auto& wanted = maximum.records[i];
            const auto& found = restored.snapshot.records[i];
            CHECK(found.id == wanted.id && found.occurrence_id == wanted.occurrence_id &&
                  found.action == wanted.action && found.minutes == wanted.minutes &&
                  found.due_utc_ms == wanted.due_utc_ms && found.paused == wanted.paused &&
                  found.state == wanted.state);
        }
    }
    maximum.next_id = 1002;
    maximum.next_occurrence_id = 1002;
    maximum.records.push_back({1001, 1001, Action::Shutdown, 1, 900000, false, OccurrenceState::Scheduled});
    CHECK(!stress.save(maximum));
    CHECK(stress.load().snapshot.records.size() == 1000);

    // Largest integer counters and deadline are valid; incrementing/overflow belongs to the engine gate.
    const auto largest = std::numeric_limits<std::uint64_t>::max();
    ScheduleSnapshot saturated{largest, largest,
        {{largest - 1, largest - 1, Action::Hibernate, 10080, largest, true, OccurrenceState::Uncertain}}};
    CHECK(storage.save(saturated));
    const auto saturation = storage.load();
    CHECK(saturation.status == LoadStatus::Loaded);
    CHECK(saturation.snapshot.next_id == largest);
    CHECK(saturation.snapshot.records.front().due_utc_ms == largest);
    CHECK(saturation.snapshot.records.front().state == OccurrenceState::Uncertain);
    // Engine initialization is the public gate: invalid storage is never overwritten.
    const std::string damaged = "damaged configuration\n";
    {
        std::ofstream out(corrupt_file, std::ios::binary | std::ios::trunc);
        out << damaged;
    }
    ScheduleBook blocked(corrupt);
    CHECK(!blocked.initialize({1000, 1000}));
    CHECK(blocked.error() == BookError::InvalidData);
    CHECK(!blocked.writable());
    CHECK(!blocked.create(Action::Sleep, L"1", {1000, 1000}));
    CHECK(blocked.tick({1000000, 1000000}).empty());
    CHECK(corrupt.load().status == LoadStatus::Invalid);
    {
        std::ifstream in(corrupt_file, std::ios::binary);
        const std::string preserved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(preserved == damaged);
    }

    // A process restart after durable intention cannot deliver the same occurrence again.
    const auto recovery_file = fixture / "recovery.pt";
    std::filesystem::remove(recovery_file);
    FileScheduleStorage recovery(recovery_file.wstring());
    CHECK(recovery.save(expected));
    ScheduleBook first_session(recovery);
    CHECK(first_session.initialize({1000, 1000}));
    const auto delivered = first_session.tick({901000, 901000});
    CHECK(delivered.size() == 1);
    CHECK(recovery.load().snapshot.records.front().state == OccurrenceState::Uncertain);
    ScheduleBook restarted(recovery);
    CHECK(restarted.initialize({902000, 2000}));
    CHECK(restarted.tick({999999, 999999}).empty());
    CHECK(restarted.list().front().state == OccurrenceState::Uncertain);

    // Abandoned temporary content never gets promoted automatically.
    const auto abandoned_file = fixture / "abandoned.pt";
    std::filesystem::remove(abandoned_file);
    {
        std::ofstream out(std::filesystem::path(abandoned_file.wstring() + L".tmp-interrupted"), std::ios::binary | std::ios::trunc);
        out << canonical;
    }
    FileScheduleStorage abandoned(abandoned_file.wstring());
    CHECK(abandoned.load().status == LoadStatus::Missing);
    CHECK(abandoned.save({}));
    CHECK(abandoned.load().status == LoadStatus::Loaded);
    CHECK(abandoned.load().snapshot.records.empty());
    std::cout << checks << " storage checks passed\n";
}
