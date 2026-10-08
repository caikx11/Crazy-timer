#include "../src/scheduler.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

using namespace power_timer;
static unsigned checks = 0;
#define CHECK(expression) do { ++checks; if (!(expression)) { \
    std::cerr << "FAIL line " << __LINE__ << ": " << #expression << '\n'; \
    std::exit(EXIT_FAILURE); } } while (false)

int main() {
    Scheduler timer;
    CHECK(timer.state() == State::Idle);
    CHECK(!timer.tick(100));
    timer.cancel();
    timer.on_resume(100);
    CHECK(timer.state() == State::Idle);

    for (auto action : {Action::Shutdown, Action::Sleep, Action::Hibernate}) {
        CHECK(timer.schedule(action, 2, 1000));
        CHECK(timer.state() == State::Scheduled);
        CHECK(timer.deadline_ms() == 121000);
        CHECK(timer.action() == action);
        CHECK(timer.remaining_ms(1000) == 120000);
        CHECK(!timer.tick(60999));
        CHECK(timer.state() == State::Scheduled);
        CHECK(!timer.tick(61000));
        CHECK(timer.state() == State::Warning);
        CHECK(!timer.tick(120999));
        CHECK(timer.tick(121000) == action);
        CHECK(timer.state() == State::Requested);
        CHECK(timer.remaining_ms(121000) == 0);
        CHECK(!timer.tick(121000));
        CHECK(!timer.tick(130000));
        timer.cancel();
        timer.on_resume(130000);
        CHECK(timer.state() == State::Requested);
    }

    CHECK(timer.schedule(Action::Sleep, 1, 0));
    CHECK(timer.state() == State::Warning);
    timer.cancel();
    CHECK(timer.state() == State::Cancelled);
    CHECK(!timer.tick(60000));
    CHECK(timer.remaining_ms(1) == 0);
    CHECK(timer.schedule(Action::Sleep, 10, 0));
    timer.cancel();
    timer.cancel();
    CHECK(!timer.tick(600000));

    for (auto lateness : {0ULL, 1ULL, 4999ULL, 5000ULL, 5001ULL, 90000ULL}) {
        CHECK(timer.schedule(Action::Shutdown, 1, 0));
        const auto requested = timer.tick(60000 + lateness);
        CHECK(requested.has_value() == (lateness <= 5000));
        CHECK(timer.state() == (lateness <= 5000 ? State::Requested : State::Expired));
        CHECK(!timer.tick(60000 + lateness));
    }

    for (auto resumed_at : {59999ULL, 60000ULL, 60001ULL, 65000ULL, 90000ULL}) {
        CHECK(timer.schedule(Action::Hibernate, 1, 0));
        timer.on_resume(resumed_at);
        CHECK(timer.state() == (resumed_at < 60000 ? State::Warning : State::Expired));
        if (resumed_at < 60000) CHECK(timer.tick(60000) == Action::Hibernate);
        else CHECK(!timer.tick(resumed_at));
    }
    CHECK(timer.schedule(Action::Sleep, 10, 0));
    timer.on_resume(1000);
    CHECK(timer.state() == State::Scheduled);
    CHECK(!timer.tick(1000));

    // An obsolete timer callback checks current state, not an old captured action.
    CHECK(timer.schedule(Action::Shutdown, 1, 0));
    CHECK(timer.schedule(Action::Hibernate, 2, 1000));
    CHECK(!timer.tick(60000));
    CHECK(timer.tick(121000) == Action::Hibernate);
    CHECK(!timer.tick(121001));

    CHECK(timer.schedule(Action::Sleep, 2, 10));
    for (auto invalid : {0ULL, 10081ULL, std::numeric_limits<unsigned long long>::max()}) {
        CHECK(!timer.schedule(Action::Shutdown, invalid, 0));
        CHECK(timer.deadline_ms() == 120010);
        CHECK(timer.action() == Action::Sleep);
        CHECK(timer.state() == State::Scheduled);
    }
    CHECK(!timer.schedule(static_cast<Action>(99), 1, 0));
    CHECK(timer.action() == Action::Sleep);
    const auto max = std::numeric_limits<std::uint64_t>::max();
    CHECK(!timer.schedule(Action::Shutdown, 1, max - 59999));
    CHECK(timer.deadline_ms() == 120010);
    CHECK(timer.schedule(Action::Hibernate, 1, max - 60000));
    CHECK(timer.deadline_ms() == max);
    CHECK(!timer.tick(max - 1));
    CHECK(timer.tick(max) == Action::Hibernate);
    CHECK(!timer.tick(max));
    CHECK(timer.schedule(Action::Shutdown, Scheduler::max_minutes, 0));
    CHECK(timer.deadline_ms() == 604800000);
    CHECK(timer.tick(604800000) == Action::Shutdown);
    std::cout << "PASS: " << checks << " deterministic checks; no power APIs linked.\n";
}
