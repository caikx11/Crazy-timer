#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <sddl.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "../scheduler.hpp"
#include "../strings.hpp"

namespace power_timer::windows {
inline constexpr wchar_t window_class[] = L"PowerTimer.Simulation.Panel";
inline constexpr UINT tray_message = WM_APP + 1;
inline constexpr UINT show_message = WM_APP + 2;
inline constexpr UINT stop_message = WM_APP + 3;
inline constexpr UINT_PTR timer_id = 1;
inline HANDLE stop_event{}, ready_event{}, ui_thread{};
inline std::atomic<HWND> host_window{};
inline std::atomic_bool initialized{};
inline std::atomic_bool user_requested_exit{};
inline bool show_on_start = true;
inline bool dedicated_process = false;
inline void (*log_message)(const wchar_t*) = nullptr;

enum ControlId { action_id = 100, duration_id, schedule_id, cancel_id,
                 preset_id = 110, open_id = 120, settings_id, visibility_id, exit_id, menu_id };

inline void set_text(HWND window, const std::wstring& value) {
    if (!window) return;
    const int count = GetWindowTextLengthW(window);
    std::wstring old(static_cast<size_t>(count) + 1, L'\0');
    GetWindowTextW(window, old.data(), count + 1);
    old.resize(count);
    if (old != value) SetWindowTextW(window, value.c_str());
}

inline std::wstring local_time_after(std::uint64_t milliseconds) {
    FILETIME file{};
    GetSystemTimeAsFileTime(&file);
    ULARGE_INTEGER ticks{};
    ticks.LowPart = file.dwLowDateTime;
    ticks.HighPart = file.dwHighDateTime;
    ticks.QuadPart += milliseconds * 10000;
    file = {ticks.LowPart, ticks.HighPart};
    SYSTEMTIME utc{}, local{};
    if (!FileTimeToSystemTime(&file, &utc) ||
        !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return text::unavailable_time;
    wchar_t date[96]{}, time[96]{};
    if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr, date, 96, nullptr) ||
        !GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, time, 96))
        return text::unavailable_time;
    return std::wstring(date) + text::separator + time;
}

inline HICON make_icon() {
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 32;
    info.bmiHeader.biHeight = -32;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits{};
    HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!color) return nullptr;
    auto* pixels = static_cast<DWORD*>(bits);
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 32; ++x) {
        const double dx = x - 15.5, dy = y - 15.5;
        const double radius = std::sqrt(dx * dx + dy * dy);
        const bool clock = (radius > 9.0 && radius < 11.0) ||
            (x >= 15 && x <= 16 && y >= 8 && y <= 16) ||
            (x >= 16 && x <= 22 && y >= 15 && y <= 16);
        pixels[y * 32 + x] = radius > 15.3 ? 0 : clock ? 0xFFFFFFFF : 0xFF2563EB;
    }
    HBITMAP mask = CreateBitmap(32, 32, 1, 1, nullptr);
    ICONINFO icon_info{TRUE, 0, 0, mask, color};
    HICON icon = mask ? CreateIconIndirect(&icon_info) : nullptr;
    if (mask) DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

// All scheduler and UI state belongs to this one message-loop thread.
class Panel {
    struct Item { HWND handle; int x, y, width, height, font; };
    std::vector<Item> items_;
    HFONT fonts_[4]{};
    HBRUSH background_{};
    HICON icon_{};
    HWND window_{}, action_{}, duration_{}, preview_{}, status_{}, countdown_{}, detail_{}, error_{}, start_{}, cancel_{};
    Scheduler scheduler_;
    NOTIFYICONDATAW tray_{};
    UINT taskbar_created_{};
    UINT dpi_ = 96;
    bool tray_added_ = false, warning_shown_ = false;
    std::uint64_t last_tray_try_{};
    std::wstring planned_;
    COLORREF bg_ = RGB(247, 249, 252), ink_ = RGB(24, 35, 53), accent_ = RGB(37, 99, 235);
    bool high_contrast_ = false;

    int px(int value) const { return MulDiv(value, static_cast<int>(dpi_), 96); }
    HWND add(const wchar_t* type, const wchar_t* label, DWORD style, int id,
             int x, int y, int width, int height, int font = 0, DWORD ex = 0) {
        HWND control = CreateWindowExW(ex, type, label, WS_CHILD | WS_VISIBLE | style,
            px(x), px(y), px(width), px(height), window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        if (!control) throw GetLastError();
        items_.push_back({control, x, y, width, height, font});
        return control;
    }
    void fonts_and_layout() {
        for (auto& font : fonts_) { if (font) DeleteObject(font); font = nullptr; }
        const int sizes[] = {14, 25, 11, 40};
        for (int i = 0; i < 4; ++i)
            fonts_[i] = CreateFontW(-px(sizes[i]), 0, 0, 0, i == 1 || i == 3 ? FW_SEMIBOLD : FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        for (const auto& item : items_) {
            MoveWindow(item.handle, px(item.x), px(item.y), px(item.width), px(item.height), TRUE);
            SendMessageW(item.handle, WM_SETFONT, reinterpret_cast<WPARAM>(fonts_[item.font]), TRUE);
        }
    }
    void colors() {
        HIGHCONTRASTW contrast{sizeof(contrast), 0, nullptr};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        high_contrast_ = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
        bg_ = high_contrast_ ? GetSysColor(COLOR_WINDOW) : RGB(247, 249, 252);
        ink_ = high_contrast_ ? GetSysColor(COLOR_WINDOWTEXT) : RGB(24, 35, 53);
        accent_ = high_contrast_ ? GetSysColor(COLOR_HIGHLIGHT) : RGB(37, 99, 235);
        if (background_) DeleteObject(background_);
        background_ = CreateSolidBrush(bg_);
        if (window_) RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    void controls() {
        add(L"STATIC", text::brand, 0, 0, 24, 22, 270, 36, 1);
        add(L"STATIC", text::simulation, SS_RIGHT, 0, 304, 30, 102, 20, 2);
        add(L"STATIC", text::subtitle, 0, 0, 24, 62, 290, 22);
        add(L"BUTTON", text::options, WS_TABSTOP | BS_OWNERDRAW, menu_id, 334, 60, 72, 28, 2);
        add(L"STATIC", text::action_label, 0, 0, 24, 96, 382, 22);
        action_ = add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                      action_id, 24, 121, 382, 180);
        for (const auto* action : text::actions) SendMessageW(action_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(action));
        SendMessageW(action_, CB_SETCURSEL, 0, 0);
        add(L"STATIC", text::duration_label, 0, 0, 24, 162, 382, 22);
        duration_ = add(L"EDIT", L"30", WS_TABSTOP | ES_AUTOHSCROLL, duration_id,
                        24, 187, 382, 34, 0, WS_EX_CLIENTEDGE);
        SendMessageW(duration_, EM_SETLIMITTEXT, 32, 0);
        add(L"STATIC", text::duration_hint, 0, 0, 24, 226, 382, 18, 2);
        for (int i = 0; i < 4; ++i)
            add(L"BUTTON", text::presets[i], WS_TABSTOP | BS_OWNERDRAW, preset_id + i,
                24 + 98 * i, 252, 88, 32, 2);
        preview_ = add(L"STATIC", L"", 0, 0, 24, 298, 382, 36, 2);
        start_ = add(L"BUTTON", text::schedule, WS_TABSTOP | BS_OWNERDRAW, schedule_id,
                     24, 338, 382, 42);
        status_ = add(L"STATIC", text::idle, SS_CENTER, 0, 24, 400, 382, 24);
        countdown_ = add(L"STATIC", text::clock_empty, SS_CENTER, 0, 24, 427, 382, 56, 3);
        detail_ = add(L"STATIC", text::idle_detail, SS_CENTER, 0, 24, 483, 382, 32, 2);
        cancel_ = add(L"BUTTON", text::cancel, WS_TABSTOP | BS_OWNERDRAW, cancel_id,
                      24, 523, 382, 36);
        error_ = add(L"STATIC", L"", SS_CENTER, 0, 24, 568, 382, 34, 2);
        add(L"STATIC", text::simulation_note, SS_CENTER, 0, 24, 608, 382, 22, 2);
        EnableWindow(cancel_, FALSE);
        fonts_and_layout();
    }
    std::wstring duration_text() const {
        wchar_t value[64]{};
        GetWindowTextW(duration_, value, 64);
        return value;
    }
    Action selected_action() const {
        return static_cast<Action>(SendMessageW(action_, CB_GETCURSEL, 0, 0));
    }
    void preview() {
        if (!preview_) return;
        const auto minutes = Scheduler::parse_duration(duration_text());
        const auto action = static_cast<int>(selected_action());
        if (!minutes || action < 0 || action > 2) {
            set_text(preview_, text::invalid_preview);
            return;
        }
        set_text(preview_, std::wstring(text::preview_prefix) + text::actions[action] +
            text::separator + local_time_after(*minutes * 60000));
    }
    void notify(const wchar_t* title, const wchar_t* body) {
        if (!tray_added_) return;
        NOTIFYICONDATAW notice = tray_;
        notice.uFlags = NIF_INFO;
        wcsncpy_s(notice.szInfoTitle, title, _TRUNCATE);
        wcsncpy_s(notice.szInfo, body, _TRUNCATE);
        notice.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND | NIIF_RESPECT_QUIET_TIME;
        Shell_NotifyIconW(NIM_MODIFY, &notice);
    }
    void refresh() {
        if (!countdown_) return;
        const auto now = GetTickCount64();
        if (scheduler_.tick(now)) {
            if (log_message) log_message(text::completed_detail);
            notify(text::completed, text::completed_detail);
        }
        if (scheduler_.state() == State::Warning && !warning_shown_) {
            warning_shown_ = true;
            notify(text::warning, text::countdown_notification);
            show(false);
        }
        const bool active = scheduler_.active();
        EnableWindow(cancel_, active);
        set_text(start_, active ? text::replace : text::schedule);
        if (active) {
            const auto seconds = (scheduler_.remaining_ms(now) + 999) / 1000;
            wchar_t value[48]{};
            swprintf_s(value, L"%02llu:%02llu:%02llu", seconds / 3600, (seconds / 60) % 60, seconds % 60);
            set_text(countdown_, value);
            set_text(status_, scheduler_.state() == State::Warning ? text::warning : text::active);
            set_text(detail_, std::wstring(text::actions[static_cast<int>(scheduler_.action())]) +
                text::separator + planned_);
        } else {
            set_text(countdown_, text::clock_empty);
            switch (scheduler_.state()) {
                case State::Requested: set_text(status_, text::completed); set_text(detail_, text::completed_detail); break;
                case State::Cancelled: set_text(status_, text::cancelled); set_text(detail_, text::cancelled_detail); break;
                case State::Expired: set_text(status_, text::expired); set_text(detail_, text::expired_detail); break;
                default: set_text(status_, text::idle); set_text(detail_, text::idle_detail); break;
            }
        }
        preview();
        if (!tray_added_ && now - last_tray_try_ >= 5000) add_tray();
    }
    void command(int id) {
        if (id >= preset_id && id < preset_id + 4) {
            const wchar_t* minutes[] = {L"15", L"30", L"60", L"120"};
            SetWindowTextW(duration_, minutes[id - preset_id]);
            SetFocus(duration_);
            SendMessageW(duration_, EM_SETSEL, 0, -1);
        } else if (id == schedule_id) {
            if (!scheduler_.schedule_text(selected_action(), duration_text(), GetTickCount64())) {
                set_text(error_, text::invalid_duration);
                SetFocus(duration_);
                return;
            }
            planned_ = local_time_after(scheduler_.remaining_ms(GetTickCount64()));
            warning_shown_ = false;
            set_text(error_, L"");
            refresh();
        } else if (id == cancel_id) {
            scheduler_.cancel();
            set_text(error_, L"");
            refresh();
        } else if (id == open_id || id == settings_id) {
            show(true);
            if (id == settings_id) set_text(error_, text::settings_note);
        } else if (id == visibility_id) {
            const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open",
                L"ms-settings:taskbar", nullptr, nullptr, SW_SHOWNORMAL));
            if (result <= 32) { show(true); set_text(error_, text::settings_error); }
        } else if (id == menu_id) {
            menu();
        } else if (id == exit_id) {
            user_requested_exit.store(true);
            SetEvent(stop_event);
            DestroyWindow(window_);
        }
    }
    void menu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        AppendMenuW(menu, MF_STRING, open_id, text::open);
        AppendMenuW(menu, MF_STRING, settings_id, text::settings);
        AppendMenuW(menu, MF_STRING, visibility_id, text::notification_settings);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (scheduler_.active() ? MF_ENABLED : MF_GRAYED), cancel_id, text::cancel);
        AppendMenuW(menu, MF_STRING, exit_id, text::exit);
        POINT point{};
        GetCursorPos(&point);
        SetForegroundWindow(window_);
        const int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, window_, nullptr);
        DestroyMenu(menu);
        if (chosen) command(chosen);
        if (window_) PostMessageW(window_, WM_NULL, 0, 0);
        if (tray_added_) Shell_NotifyIconW(NIM_SETFOCUS, &tray_);
    }
    void draw_button(const DRAWITEMSTRUCT& draw) {
        const bool primary = draw.CtlID == schedule_id;
        const bool disabled = (draw.itemState & ODS_DISABLED) != 0;
        const COLORREF fill = primary && !disabled ? accent_ : high_contrast_ ? bg_ : RGB(233, 239, 248);
        const COLORREF foreground = disabled ? GetSysColor(COLOR_GRAYTEXT) : primary ?
            (high_contrast_ ? GetSysColor(COLOR_HIGHLIGHTTEXT) : RGB(255, 255, 255)) : ink_;
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, fill);
        const auto old_brush = SelectObject(draw.hDC, brush);
        const auto old_pen = SelectObject(draw.hDC, pen);
        RoundRect(draw.hDC, draw.rcItem.left, draw.rcItem.top, draw.rcItem.right, draw.rcItem.bottom, px(10), px(10));
        SelectObject(draw.hDC, old_brush); SelectObject(draw.hDC, old_pen);
        DeleteObject(brush); DeleteObject(pen);
        wchar_t label[128]{};
        GetWindowTextW(draw.hwndItem, label, 128);
        SetBkMode(draw.hDC, TRANSPARENT);
        SetTextColor(draw.hDC, foreground);
        RECT rect = draw.rcItem;
        if (draw.itemState & ODS_SELECTED) OffsetRect(&rect, 0, 1);
        DrawTextW(draw.hDC, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (draw.itemState & ODS_FOCUS) { InflateRect(&rect, -px(4), -px(4)); DrawFocusRect(draw.hDC, &rect); }
    }
public:
    ~Panel() {
        if (window_ && IsWindow(window_)) DestroyWindow(window_);
        for (auto font : fonts_) if (font) DeleteObject(font);
        if (background_) DeleteObject(background_);
        if (icon_) DestroyIcon(icon_);
    }
    void show(bool activate) {
        ShowWindow(window_, activate ? SW_RESTORE : SW_SHOWNOACTIVATE);
        if (activate) { SetForegroundWindow(window_); SetFocus(duration_); }
    }
    void add_tray() {
        last_tray_try_ = GetTickCount64();
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
        if (tray_added_) {
            tray_.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &tray_);
            if (error_) set_text(error_, L"");
        } else if (error_) { set_text(error_, text::tray_error); show(false); }
    }
    bool create() {
        colors();
        icon_ = make_icon();
        taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");
        WNDCLASSW cls{};
        cls.lpfnWndProc = procedure;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.hIcon = icon_;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.lpszClassName = window_class;
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        dpi_ = GetDpiForSystem();
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        RECT size{0, 0, px(430), px(644)};
        constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRectExForDpi(&size, style, FALSE, WS_EX_CONTROLPARENT, dpi_);
        const int width = size.right - size.left, height = size.bottom - size.top;
        window_ = CreateWindowExW(WS_EX_CONTROLPARENT, window_class, text::title, style,
            work.right - width - px(20), (std::max)(work.top, work.bottom - height - px(20)),
            width, height, nullptr, nullptr, cls.hInstance, this);
        if (!window_) return false;
        // Explorer may have lower integrity than the tool host. Allow only its
        // inert tray-recreation broadcast, never commands or our WM_APP channel.
        if (!taskbar_created_ || !ChangeWindowMessageFilterEx(window_, taskbar_created_, MSGFLT_ALLOW, nullptr)) {
            if (log_message) log_message(L"TaskbarCreated message filter initialization failed.");
            DestroyWindow(window_);
            return false;
        }
        try { controls(); } catch (...) { DestroyWindow(window_); return false; }
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = window_;
        tray_.uID = 1;
        tray_.uCallbackMessage = tray_message;
        tray_.hIcon = icon_;
        wcsncpy_s(tray_.szTip, text::title, _TRUNCATE);
        if (!SetTimer(window_, timer_id, 250, nullptr)) { DestroyWindow(window_); return false; }
        add_tray();
        refresh();
        host_window.store(window_);
        if (show_on_start) show(true);
        return true;
    }
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<Panel*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            self->window_ = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self) return DefWindowProcW(window, message, wparam, lparam);
        if (message == self->taskbar_created_ && self->taskbar_created_) {
            self->tray_added_ = false; self->add_tray(); return 0;
        }
        switch (message) {
            case WM_COMMAND:
                if (LOWORD(wparam) == duration_id && HIWORD(wparam) == EN_CHANGE) {
                    self->preview();
                    if (Scheduler::parse_duration(self->duration_text())) set_text(self->error_, L"");
                }
                else if (LOWORD(wparam) == action_id && HIWORD(wparam) == CBN_SELCHANGE) self->preview();
                else if (HIWORD(wparam) == BN_CLICKED) self->command(LOWORD(wparam));
                return 0;
            case WM_TIMER: if (wparam == timer_id) self->refresh(); return 0;
            case WM_POWERBROADCAST:
                if (wparam == PBT_APMRESUMEAUTOMATIC || wparam == PBT_APMRESUMESUSPEND) {
                    self->scheduler_.on_resume(GetTickCount64()); self->refresh();
                }
                return TRUE;
            case tray_message:
                switch (LOWORD(lparam)) {
                    case NIN_SELECT: case NIN_KEYSELECT: case NIN_BALLOONUSERCLICK: self->show(true); break;
                    case WM_CONTEXTMENU: self->menu(); break;
                }
                return 0;
            case show_message: self->show(true); return 0;
            case WM_CONTEXTMENU: self->menu(); return 0;
            case stop_message: EndMenu(); DestroyWindow(window); return 0;
            case WM_CLOSE: ShowWindow(window, SW_HIDE); return 0;
            case WM_DESTROY:
                KillTimer(window, timer_id);
                self->scheduler_.cancel();
                if (self->tray_added_) Shell_NotifyIconW(NIM_DELETE, &self->tray_);
                self->tray_added_ = false;
                host_window.store(nullptr);
                self->window_ = nullptr;
                PostQuitMessage(0); return 0;
            case WM_DPICHANGED: {
                self->dpi_ = HIWORD(wparam);
                self->fonts_and_layout();
                const auto* rect = reinterpret_cast<RECT*>(lparam);
                SetWindowPos(window, nullptr, rect->left, rect->top, rect->right - rect->left,
                    rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE); return 0;
            }
            case WM_SETTINGCHANGE: self->colors(); return 0;
            case WM_CTLCOLORSTATIC:
                SetTextColor(reinterpret_cast<HDC>(wparam), self->ink_);
                SetBkColor(reinterpret_cast<HDC>(wparam), self->bg_);
                return reinterpret_cast<LRESULT>(self->background_);
            case WM_ERASEBKGND: {
                RECT rect{}; GetClientRect(window, &rect);
                FillRect(reinterpret_cast<HDC>(wparam), &rect, self->background_); return 1;
            }
            case WM_DRAWITEM: self->draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam)); return TRUE;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }
};

inline HANDLE user_mutex() {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return nullptr;
    DWORD length{};
    GetTokenInformation(token, TokenUser, nullptr, 0, &length);
    std::vector<BYTE> data(length);
    const bool ok = GetTokenInformation(token, TokenUser, data.data(), length, &length) != FALSE;
    CloseHandle(token);
    if (!ok) return nullptr;
    LPWSTR sid{};
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid, &sid)) return nullptr;
    const std::wstring name = std::wstring(L"Global\\PowerTimer.Simulation.") + sid;
    LocalFree(sid);
    return CreateMutexW(nullptr, FALSE, name.c_str());
}

inline DWORD WINAPI ui_main(void*) {
    HANDLE instance = user_mutex();
    if (!instance) { SetEvent(ready_event); return 1; }
    const DWORD locked = WaitForSingleObject(instance, 0);
    if (locked != WAIT_OBJECT_0 && locked != WAIT_ABANDONED) {
        if (HWND prior = FindWindowW(window_class, nullptr)) PostMessageW(prior, show_message, 0, 0);
        CloseHandle(instance); SetEvent(ready_event); return 0;
    }
    const auto prior_dpi = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&common);
    {
        Panel panel;
        const bool created = panel.create();
        initialized.store(created);
        SetEvent(ready_event);
        bool running = created;
        while (running) {
            const DWORD wait = MsgWaitForMultipleObjects(1, &stop_event, FALSE, INFINITE, QS_ALLINPUT);
            if (wait == WAIT_OBJECT_0 || wait == WAIT_FAILED) break;
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) { running = false; break; }
                HWND window = host_window.load();
                if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE && window) {
                    ShowWindow(window, SW_HIDE); continue;
                }
                if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN && window &&
                    (GetFocus() == GetDlgItem(window, duration_id))) {
                    PostMessageW(window, WM_COMMAND, schedule_id, 0); continue;
                }
                if (!window || !IsDialogMessageW(window, &message)) {
                    TranslateMessage(&message); DispatchMessageW(&message);
                }
            }
        }
    }
    UnregisterClassW(window_class, GetModuleHandleW(nullptr));
    if (prior_dpi) SetThreadDpiAwarenessContext(prior_dpi);
    ReleaseMutex(instance); CloseHandle(instance);
    // The engine can retain other threads in a dedicated tool process after
    // our UI thread finishes. Explicit user exit ends that process after cleanup.
    if (dedicated_process && user_requested_exit.load()) ExitProcess(0);
    return 0;
}

inline void stop() {
    if (stop_event) SetEvent(stop_event);
    if (HWND window = host_window.load()) PostMessageW(window, stop_message, 0, 0);
    if (ui_thread) { WaitForSingleObject(ui_thread, INFINITE); CloseHandle(ui_thread); ui_thread = nullptr; }
    if (ready_event) { CloseHandle(ready_event); ready_event = nullptr; }
    if (stop_event) { CloseHandle(stop_event); stop_event = nullptr; }
    initialized.store(false);
}
inline bool start(bool visible, void (*logger)(const wchar_t*) = nullptr, bool dedicated = false) {
    show_on_start = visible;
    dedicated_process = dedicated;
    user_requested_exit.store(false);
    log_message = logger;
    initialized.store(false);
    stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (stop_event && ready_event) ui_thread = CreateThread(nullptr, 0, ui_main, nullptr, 0, nullptr);
    if (!ui_thread || WaitForSingleObject(ready_event, 10000) != WAIT_OBJECT_0 || !initialized.load()) {
        stop(); return false;
    }
    return true;
}
}
