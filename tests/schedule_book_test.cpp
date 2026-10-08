#include "../src/schedule_book.hpp"
#include <cstdlib>
#include <iostream>

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
        if (fail_save) return false;
        data = {LoadStatus::Loaded, value};
        return true;
    }
};
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
        check(book.create(Action::Sleep, L"1", {100000, 0}), "up to 1000 records accepted");
    check(!book.create(Action::Sleep, L"1", {100000, 0}) && book.list().size() == 1000 &&
          book.error() == BookError::InvalidInput, "capacity failure preserves all records");
    check(book.erase(500) && book.create(Action::Sleep, L"1", {100000, 0}) &&
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
int main() {
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
