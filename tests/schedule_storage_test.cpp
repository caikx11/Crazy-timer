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
