#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <sddl.h>
#include <cstdio>
#include <string>
#include <atomic>

static std::wstring mutex_name;
static unsigned show_requests;
static unsigned panel_shows;
static unsigned tray_attempts;
static unsigned diagnostic_messages;
static std::atomic<ULONGLONG> monotonic_ms{10000};
static ULONGLONG WINAPI simulated_tick() { return monotonic_ms.load(); }
static bool duplicate = true;
static BOOL WINAPI capture_show(HWND, int);
// This test covers visibility, not Windows message-filter permissions.
static BOOL WINAPI simulated_filter(HWND, UINT, DWORD, PCHANGEFILTERSTRUCT) { return TRUE; }
static HRESULT WINAPI unavailable_data(HWND, int, HANDLE, DWORD, LPWSTR) { return E_FAIL; }
static BOOL WINAPI capture_tray(DWORD operation, PNOTIFYICONDATAW) {
    if (operation == NIM_ADD) return ++tray_attempts > 1;
    return TRUE;
}
static HANDLE WINAPI isolated_mutex(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCWSTR) {
    return CreateMutexW(attributes, owner, mutex_name.c_str());
}
static HWND WINAPI prior_window(LPCWSTR, LPCWSTR) {
    return duplicate ? reinterpret_cast<HWND>(1) : nullptr;
}
static BOOL WINAPI capture_message(HWND, UINT message, WPARAM, LPARAM) {
    if (message == WM_APP + 2) ++show_requests;
    return TRUE;
}
#define CreateMutexW isolated_mutex
#define FindWindowW prior_window
#define PostMessageW capture_message
#define ShowWindow capture_show
#define Shell_NotifyIconW capture_tray
#define SHGetFolderPathW unavailable_data
#define ChangeWindowMessageFilterEx simulated_filter
#define GetTickCount64 simulated_tick
#include "../src/windows/tray_host.hpp"
#undef CreateMutexW
#undef FindWindowW
#undef PostMessageW
#undef ShowWindow
#undef Shell_NotifyIconW
#undef SHGetFolderPathW
#undef ChangeWindowMessageFilterEx
#undef GetTickCount64

static BOOL WINAPI capture_show(HWND window, int) {
    wchar_t name[128]{};
    GetClassNameW(window, name, 128);
    if (wcscmp(name, power_timer::windows::window_class) == 0) ++panel_shows;
    return FALSE; // Tests never display any windows.
}

int main() {
    mutex_name = L"Local\\CrazyTime.StartHidden.Repro." + std::to_wstring(GetCurrentProcessId());
    HANDLE existing = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
    if (!existing) return 2;
    power_timer::windows::start(false, nullptr, false, true);
    power_timer::windows::stop();
    ReleaseMutex(existing);
    CloseHandle(existing);
    std::printf("Hidden duplicate startup: %u request(s) to open existing panel; expected 0\n", show_requests);
    if (show_requests != 0) return 1;
    existing = CreateMutexW(nullptr, TRUE, mutex_name.c_str());
    if (!existing) return 2;
    power_timer::windows::start(true, nullptr, false, true);
    power_timer::windows::stop();
    ReleaseMutex(existing);
    CloseHandle(existing);
    std::printf("Visible duplicate startup: %u open request(s); expected 1\n", show_requests);
    if (show_requests != 1) return 1;

    duplicate = false;
    if (!power_timer::windows::start(false, [](const wchar_t* message) {
        ++diagnostic_messages;
        std::fwprintf(stderr, L"Host: %ls\n", message);
    }, false, true)) { std::printf("Host initialization failed: %lu\n", GetLastError()); return 2; }
    HWND window = power_timer::windows::host_window.load();
    const unsigned initial_shows = panel_shows;
    SendMessageW(window, WM_TIMER, power_timer::windows::timer_id, 0);
    const unsigned attempts_before_delay = tray_attempts;
    monotonic_ms.fetch_add(5000);
    SendMessageW(window, WM_TIMER, power_timer::windows::timer_id, 0);
    const unsigned attempts_after_delay = tray_attempts;
    SendMessageW(window, RegisterWindowMessageW(L"TaskbarCreated"), 0, 0);
    const unsigned recovered_shows = panel_shows;
    // Explicit activation via the existing tray event must still work.
    SendMessageW(window, power_timer::windows::tray_message, 0, NIN_SELECT);
    const unsigned explicit_shows = panel_shows;
    power_timer::windows::stop();
    std::printf("Tray failure/recovery: %u/%u panel show(s); expected 0/0; attempts %u\n",
        initial_shows, recovered_shows, tray_attempts);
    std::printf("Explicit tray activation: %u additional show(s); expected 1\n", explicit_shows - recovered_shows);
    std::printf("Periodic retry attempts before/after 5 seconds: %u/%u; expected 1/2; diagnostics %u\n",
        attempts_before_delay, attempts_after_delay, diagnostic_messages);
    return initial_shows == 0 && recovered_shows == 0 && tray_attempts == 3 &&
        attempts_before_delay == 1 && attempts_after_delay == 2 && diagnostic_messages == 1 &&
        explicit_shows == recovered_shows + 1 ? 0 : 1;
}
