#include "../src/warning_tracker.hpp"
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace power_timer;
static int checks = 0;
static void check(bool value, const char* what) {
    ++checks;
    if (!value) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); }
}
// Counts down one second at a time from `start_ms` and returns the indexes announced.
static std::vector<std::size_t> run(WarningTracker& tracker, std::uint64_t occurrence, std::uint64_t start_ms) {
    std::vector<std::size_t> fired;
    for (std::uint64_t remaining = start_ms; remaining > 0; remaining -= 1000)
        if (const auto index = tracker.due(occurrence, remaining)) fired.push_back(*index);
    return fired;
}
int main() {
    {
        WarningTracker tracker;
        const auto fired = run(tracker, 1, 120000);
        check(fired.size() == 1 && fired[0] == 4, "2 minute timer warns once, at 1 minute");
    }
    {
        WarningTracker tracker;
        const auto fired = run(tracker, 1, 5 * 60000);
        check(fired.size() == 1 && fired[0] == 4, "5 minute timer skips the 5 minute warning it started on");
    }
    {
        WarningTracker tracker;
        const auto fired = run(tracker, 1, 20 * 60000);
        check(fired == std::vector<std::size_t>({1, 2, 3, 4}), "20 minute timer warns at 15, 10, 5 and 1");
    }
    {
        WarningTracker tracker;
        const auto fired = run(tracker, 1, 90 * 60000 + 1000);
        check(fired == std::vector<std::size_t>({0, 1, 2, 3, 4}), "long timer warns at every threshold");
    }
    {
        // Postponed from 1 minute left to 11 minutes: 10, 5 and 1 minute warnings come back once.
        WarningTracker tracker;
        run(tracker, 1, 120000);
        tracker.reopen_ahead_of(1, 11 * 60000);
        const auto again = run(tracker, 1, 11 * 60000);
        check(again == std::vector<std::size_t>({2, 3, 4}), "postponement re-arms only upcoming warnings");
    }
    std::cout << checks << " warning tracker checks passed\n";
}
