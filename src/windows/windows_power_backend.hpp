#pragma once

#include <windows.h>
#include "../power_executor.hpp"

namespace power_timer::windows {

// Real Windows power actions, used by the mod; the standalone preview simulates.
// powrprof.dll is resolved at run time so the mod needs no
// extra import library.
class WindowsPowerBackend : public PowerBackend {
public:
    PowerCapabilities query() override {
        PowerCapabilities caps; // shutdown is always attempted; privilege failure is reported by request()
        SYSTEM_POWER_CAPABILITIES power{};
        const auto get = functions().get_capabilities;
        if (!get) { caps.sleep = caps.hibernate = Availability::QueryFailed; return caps; }
        if (!get(&power)) { caps.sleep = caps.hibernate = Availability::QueryFailed; return caps; }
        // Classic S1-S3 sleep, or Modern Standby (SetSuspendState also enters it).
        const bool sleep_supported = power.SystemS1 || power.SystemS2 || power.SystemS3 || modern_standby(power);
        caps.sleep = sleep_supported ? Availability::Available : Availability::Unsupported;
        // Hibernate needs S4 support and an enabled hibernation file.
        caps.hibernate = !power.SystemS4 ? Availability::Unsupported
            : !power.HiberFilePresent ? Availability::DisabledByPolicy : Availability::Available;
        return caps;
    }

    ExecResult request(Action action, bool force) override {
        Privilege privilege;
        if (!privilege.acquired())
            return {ExecStatus::PrivilegeFailed, Availability::Available, privilege.error()};
        SetLastError(ERROR_SUCCESS);
        BOOL ok = FALSE;
        switch (action) {
            case Action::Shutdown: {
                // Normal shutdown lets applications object; EWX_FORCE only with the schedule's explicit opt-in.
                const UINT flags = EWX_POWEROFF | (force ? EWX_FORCE : EWX_FORCEIFHUNG);
                ok = ExitWindowsEx(flags, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED);
                break;
            }
            case Action::Sleep:
            case Action::Hibernate: {
                const auto suspend = functions().set_suspend_state;
                if (!suspend) return {ExecStatus::Rejected, Availability::Available, static_cast<std::uint32_t>(ERROR_PROC_NOT_FOUND)};
                // Never force and never disable wake events. Blocks until the PC resumes.
                ok = suspend(action == Action::Hibernate ? TRUE : FALSE, FALSE, FALSE);
                break;
            }
        }
        if (!ok) return {ExecStatus::Rejected, Availability::Available, static_cast<std::uint32_t>(GetLastError())};
        return {ExecStatus::Accepted, Availability::Available, 0};
    }

private:
    using GetCapsFn = BOOLEAN(WINAPI*)(PSYSTEM_POWER_CAPABILITIES);
    using SuspendFn = BOOLEAN(WINAPI*)(BOOLEAN, BOOLEAN, BOOLEAN);
    struct Functions { GetCapsFn get_capabilities = nullptr; SuspendFn set_suspend_state = nullptr; };
    static const Functions& functions() {
        static const Functions loaded = [] {
            Functions result;
            if (const auto module = LoadLibraryExW(L"powrprof.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) {
                result.get_capabilities = reinterpret_cast<GetCapsFn>(reinterpret_cast<void*>(GetProcAddress(module, "GetPwrCapabilities")));
                result.set_suspend_state = reinterpret_cast<SuspendFn>(reinterpret_cast<void*>(GetProcAddress(module, "SetSuspendState")));
            }
            return result;
        }();
        return loaded;
    }
    // Older SDK headers lack AoAc; then Modern Standby cannot be told apart from "no sleep",
    // so sleep is attempted and request() reports a real failure if it is unsupported.
    template <class Capabilities>
    static bool modern_standby(const Capabilities& power) {
        if constexpr (requires { power.AoAc; }) return power.AoAc != FALSE;
        else return true;
    }

    // Enables SeShutdownPrivilege for the call and restores the previous state on every path.
    class Privilege {
    public:
        Privilege() {
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token_)) {
                error_ = GetLastError();
                return;
            }
            TOKEN_PRIVILEGES wanted{};
            wanted.PrivilegeCount = 1;
            wanted.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &wanted.Privileges[0].Luid)) {
                error_ = GetLastError();
                return;
            }
            DWORD returned = 0;
            SetLastError(ERROR_SUCCESS);
            const BOOL adjusted = AdjustTokenPrivileges(token_, FALSE, &wanted, sizeof(previous_), &previous_, &returned);
            const DWORD last = GetLastError();
            // AdjustTokenPrivileges can "succeed" while assigning nothing (ERROR_NOT_ALL_ASSIGNED).
            if (!adjusted) { error_ = last; return; }
            if (last == ERROR_NOT_ALL_ASSIGNED) { error_ = last; return; }
            changed_ = returned != 0 && previous_.PrivilegeCount != 0;
            ok_ = true;
        }
        ~Privilege() {
            if (token_) {
                if (changed_) AdjustTokenPrivileges(token_, FALSE, &previous_, 0, nullptr, nullptr);
                CloseHandle(token_);
            }
        }
        Privilege(const Privilege&) = delete;
        Privilege& operator=(const Privilege&) = delete;
        bool acquired() const { return ok_; }
        std::uint32_t error() const { return static_cast<std::uint32_t>(error_); }
    private:
        HANDLE token_{};
        TOKEN_PRIVILEGES previous_{};
        bool ok_ = false, changed_ = false;
        DWORD error_ = 0;
    };
};

} // namespace power_timer::windows
