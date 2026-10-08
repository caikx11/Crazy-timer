#pragma once

#include <cstdint>
#include <limits>
#include <optional>

namespace power_timer {

enum class Action { Shutdown, Sleep, Hibernate };
enum class State { Idle, Scheduled, Warning, Requested, Cancelled, Expired };

// Pure state machine. Times are monotonic milliseconds from a caller-owned clock.
// It emits an intention only; it never performs a system power operation.
class Scheduler {
public:
    static constexpr std::uint64_t min_minutes = 1;
    static constexpr std::uint64_t max_minutes = 10080;
    static constexpr std::uint64_t warning_ms = 60000;
    static constexpr std::uint64_t max_lateness_ms = 5000;

    // Invalid input leaves any existing schedule intact.
    [[nodiscard]] bool schedule(Action action, std::uint64_t minutes,
                                std::uint64_t now_ms) noexcept {
        if (!valid_action(action) || minutes < min_minutes || minutes > max_minutes)
            return false;
        const auto duration_ms = minutes * 60000;
        if (now_ms > std::numeric_limits<std::uint64_t>::max() - duration_ms)
            return false;
        action_ = action;
        deadline_ = now_ms + duration_ms;
        state_ = duration_ms <= warning_ms ? State::Warning : State::Scheduled;
        return true;
    }

    [[nodiscard]] std::optional<Action> tick(std::uint64_t now_ms) noexcept {
        if (!active()) return std::nullopt;
        if (now_ms >= deadline_) {
            if (now_ms - deadline_ > max_lateness_ms) {
                state_ = State::Expired;
                return std::nullopt;
            }
            state_ = State::Requested;
            return action_;
        }
        state_ = deadline_ - now_ms <= warning_ms ? State::Warning : State::Scheduled;
        return std::nullopt;
    }

    // A resume notification must be delivered before a timer tick after waking.
    // Never issue a missed power action merely because the computer woke up.
    void on_resume(std::uint64_t now_ms) noexcept {
        if (!active()) return;
        if (now_ms >= deadline_) {
            state_ = State::Expired;
        } else {
            state_ = deadline_ - now_ms <= warning_ms ? State::Warning : State::Scheduled;
        }
    }

    void cancel() noexcept {
        if (active()) state_ = State::Cancelled;
    }

    [[nodiscard]] State state() const noexcept { return state_; }
    [[nodiscard]] bool active() const noexcept {
        return state_ == State::Scheduled || state_ == State::Warning;
    }
    [[nodiscard]] std::uint64_t deadline_ms() const noexcept { return deadline_; }
    [[nodiscard]] Action action() const noexcept { return action_; }
    [[nodiscard]] std::uint64_t remaining_ms(std::uint64_t now_ms) const noexcept {
        return active() && now_ms < deadline_ ? deadline_ - now_ms : 0;
    }

private:
    static constexpr bool valid_action(Action action) noexcept {
        return action == Action::Shutdown || action == Action::Sleep ||
               action == Action::Hibernate;
    }
    State state_ = State::Idle;
    Action action_ = Action::Shutdown;
    std::uint64_t deadline_ = 0;
};

} // namespace power_timer
