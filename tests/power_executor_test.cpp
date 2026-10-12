#include "../src/power_executor.hpp"
#include <cstdlib>
#include <iostream>

using namespace power_timer;
static void check(bool value, const char* what) {
    if (!value) { std::cerr << "FAIL: " << what << '\n'; std::exit(1); }
}
struct Fake : PowerBackend {
    PowerCapabilities caps;
    ExecResult result{ExecStatus::Accepted, Availability::Available, 0};
    int queries = 0, requests = 0;
    PowerCapabilities query() override { ++queries; return caps; }
    ExecResult request(Action) override { ++requests; return result; }
};
int main() {
    {
        Fake fake;
        const auto r = execute_power_action(fake, Action::Sleep);
        check(r.status == ExecStatus::Accepted && fake.queries == 1 && fake.requests == 1, "accepted after capability check");
    }
    for (auto action : {Action::Shutdown, Action::Sleep, Action::Hibernate}) {
        Fake fake;
        (action == Action::Shutdown ? fake.caps.shutdown : action == Action::Sleep ? fake.caps.sleep : fake.caps.hibernate) =
            Availability::ModernStandbyOnly;
        const auto r = execute_power_action(fake, action);
        check(r.status == ExecStatus::NotAvailable && r.availability == Availability::ModernStandbyOnly &&
              fake.requests == 0, "unavailable action is explained and not requested");
    }
    {
        Fake fake;
        fake.result = {ExecStatus::PrivilegeFailed, Availability::Available, 1300};
        const auto r = execute_power_action(fake, Action::Shutdown);
        check(r.status == ExecStatus::PrivilegeFailed && r.error_code == 1300 && fake.requests == 1, "privilege error propagated, no retry");
    }
    {
        Fake fake;
        fake.caps.hibernate = Availability::QueryFailed;
        check(execute_power_action(fake, Action::Hibernate).availability == Availability::QueryFailed, "query failure reported");
        check(execute_power_action(fake, Action::Shutdown).status == ExecStatus::Accepted, "other actions unaffected");
    }
    std::cout << "power_executor ok\n";
}
