#pragma once

#include "scheduler.hpp"
#include <cstdint>

namespace power_timer {

// Why an action cannot run now. Shown to the user next to the schedule.
enum class Availability { Available, Unsupported, ModernStandbyOnly, DisabledByPolicy, QueryFailed };
struct PowerCapabilities {
    Availability shutdown = Availability::Available;
    Availability sleep = Availability::Available;
    Availability hibernate = Availability::Available;
    Availability of(Action action) const {
        switch (action) {
            case Action::Shutdown: return shutdown;
            case Action::Sleep: return sleep;
            case Action::Hibernate: return hibernate;
        }
        return Availability::Unsupported;
    }
};
enum class ExecStatus {
    Accepted,          // The system accepted the request; completion is not proven.
    NotAvailable,      // Capability check failed; nothing was requested.
    PrivilegeFailed,   // Enabling the temporary privilege failed (partly or fully).
    Rejected           // The system call itself reported an error.
};
struct ExecResult { ExecStatus status; Availability availability; std::uint32_t error_code; };

// The only seam to the operating system. The simulated build never provides a
// real implementation; tests use fakes. A real one needs deliberate manual testing.
class PowerBackend {
public:
    virtual ~PowerBackend() = default;
    virtual PowerCapabilities query() = 0;
    // Must restore any privilege it enabled, on every path, and return the real error.
    virtual ExecResult request(Action action, bool force) = 0;
};

// Checks capability right before the request; never retries. Applications are
// forced to close only when the schedule explicitly opted in, and only for Shutdown.
inline ExecResult execute_power_action(PowerBackend& backend, Action action, bool force = false) {
    const auto availability = backend.query().of(action);
    if (availability != Availability::Available)
        return {ExecStatus::NotAvailable, availability, 0};
    return backend.request(action, force && action == Action::Shutdown);
}

// Simulation target: reports everything available and accepts without touching
// the system. Physically separate from any real executor.
class SimulatedBackend : public PowerBackend {
public:
    PowerCapabilities query() override { return {}; }
    ExecResult request(Action, bool) override { return {ExecStatus::Accepted, Availability::Available, 0}; }
};

} // namespace power_timer
