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
#include <shlobj.h>
#include <sddl.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_set>
#include "../scheduler.hpp"
#include "../schedule_book.hpp"
#include "schedule_file.hpp"
#include "local_calendar.hpp"
#include "../strings.hpp"
#include "../strings_en.hpp"

namespace power_timer::windows {
inline const wchar_t* get_text(unsigned locale, const wchar_t* pt_br, const wchar_t* en) {
    return locale == 1 ? en : pt_br;
}
inline constexpr wchar_t window_class[] = L"PowerTimer.Simulation.Panel";
inline constexpr UINT tray_message = WM_APP + 1;
inline constexpr UINT show_message = WM_APP + 2;
inline constexpr UINT stop_message = WM_APP + 3;
inline constexpr UINT sound_message = WM_APP + 4; // wparam: 0 off, 1 on
inline constexpr UINT_PTR timer_id = 1;
inline HANDLE stop_event{}, ready_event{}, ui_thread{};
inline std::atomic<HWND> host_window{};
inline std::atomic_bool initialized{};
inline std::atomic_bool user_requested_exit{};
inline bool show_on_start = true;
inline bool dedicated_process = false;
inline void (*log_message)(const wchar_t*) = nullptr;

enum ControlId { action_id = 100, duration_id, schedule_id, cancel_id,
                 preset_id = 110, open_id = 120, settings_id, visibility_id, exit_id, menu_id,
                 list_id = 130, edit_id, pause_id, erase_id, new_id,
                 kind_id = 140, date_id, time_id, daily_id, weekday_id = 150, theme_id = 160, locale_id = 170 };

inline ClockSample clock_sample() {
    FILETIME file{};
    GetSystemTimeAsFileTime(&file);
    ULARGE_INTEGER ticks{};
    ticks.LowPart = file.dwLowDateTime; ticks.HighPart = file.dwHighDateTime;
    return {(ticks.QuadPart - 116444736000000000ULL) / 10000, GetTickCount64()};
}

inline std::wstring data_file() {
    wchar_t local[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local))) return L"";
    const auto directory = std::wstring(local) + L"\\PowerTimer";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return L"";
    return directory + (dedicated_process ? L"\\schedules-v1.dat" : L"\\preview-v1.dat");
}

inline void set_text(HWND window, const std::wstring& value) {
    if (!window) return;
    const int count = GetWindowTextLengthW(window);
    std::wstring old(static_cast<size_t>(count) + 1, L'\0');
    GetWindowTextW(window, old.data(), count + 1);
    old.resize(count);
    if (old != value) SetWindowTextW(window, value.c_str());
}

inline std::wstring local_time_at(std::uint64_t utc_ms, unsigned locale) {
    const auto civil = WindowsCalendar{}.local(utc_ms);
    if (!civil) return get_text(locale, text::unavailable_time, text_en::unavailable_time);
    SYSTEMTIME local{};
    local.wYear = static_cast<WORD>(civil->year); local.wMonth = static_cast<WORD>(civil->month);
    local.wDay = static_cast<WORD>(civil->day); local.wHour = static_cast<WORD>(civil->hour);
    local.wMinute = static_cast<WORD>(civil->minute);
    wchar_t date[96]{}, time[96]{};
    if (!GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, DATE_SHORTDATE, &local, nullptr, date, 96, nullptr) ||
        !GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &local, nullptr, time, 96))
        return get_text(locale, text::unavailable_time, text_en::unavailable_time);
    return std::wstring(date) + get_text(locale, text::separator, text_en::separator) + time;
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
        pixels[y * 32 + x] = radius > 15.3 ? 0 : clock ? 0xFFFFFFFF : 0xFF0078D4;
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
    HWND window_{}, action_{}, duration_{}, preview_{}, status_{}, countdown_{}, detail_{}, error_{}, start_{}, cancel_{}, theme_combo_{}, locale_combo_{};
    FileScheduleStorage storage_;
    WindowsCalendar calendar_;
    ScheduleBook book_;
    HWND kind_{}, date_{}, time_{}, days_[7]{};
    std::vector<HWND> duration_controls_, once_controls_, weekly_controls_, calendar_controls_;
    HWND list_{}, edit_{}, pause_{}, erase_{};
    std::uint64_t selected_id_{}, editing_id_{};
    std::vector<std::uint64_t> displayed_ids_;
    std::wstring list_signature_;
    std::wstring conflict_signature_;
    bool rebuilding_list_ = false;
    std::unordered_set<std::uint64_t> warned_;
    std::unordered_set<std::uint64_t> warned_thresholds_; // occurrence_id * 10 + threshold_index
    NOTIFYICONDATAW tray_{};
    UINT taskbar_created_{};
    UINT dpi_ = 96;
    bool tray_added_ = false;
    std::uint64_t last_tray_try_{};
    COLORREF bg_ = RGB(247, 249, 252), ink_ = RGB(24, 35, 53), accent_ = RGB(0, 120, 212);
    bool high_contrast_ = false;
    unsigned theme_value_ = 0; // 0=automatic, 1=light, 2=dark
    unsigned locale_ = 0; // 0=pt-BR, 1=English

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
    void refresh_locale_text() {
        if (!window_) return;

        // Update window title
        SetWindowTextW(window_, get_text(locale_, text::title, text_en::title));

        // Update static text controls
        SetWindowTextW(GetDlgItem(window_, 0), get_text(locale_, text::brand, text_en::brand)); // Brand
        SetWindowTextW(GetDlgItem(window_, 1), get_text(locale_, text::simulation, text_en::simulation)); // Simulation
        SetWindowTextW(GetDlgItem(window_, 2), get_text(locale_, text::subtitle, text_en::subtitle)); // Subtitle
        SetWindowTextW(GetDlgItem(window_, 3), get_text(locale_, text::options, text_en::options)); // Options button
        SetWindowTextW(GetDlgItem(window_, 4), get_text(locale_, text::theme_label, text_en::theme_label)); // Tema label
        SetWindowTextW(GetDlgItem(window_, 5), get_text(locale_, text::language_label, text_en::language_label)); // Idioma label
        SetWindowTextW(GetDlgItem(window_, 6), get_text(locale_, text::action_label, text_en::action_label)); // Action label
        SetWindowTextW(GetDlgItem(window_, 7), get_text(locale_, text::kind_label, text_en::kind_label)); // Kind label
        SetWindowTextW(GetDlgItem(window_, 8), get_text(locale_, text::duration_label, text_en::duration_label)); // Duration label
        SetWindowTextW(GetDlgItem(window_, 9), get_text(locale_, text::date_label, text_en::date_label)); // Date label
        SetWindowTextW(GetDlgItem(window_, 10), get_text(locale_, text::time_label, text_en::time_label)); // Time label
        SetWindowTextW(GetDlgItem(window_, 11), get_text(locale_, text::weekly_label, text_en::weekly_label)); // Weekly label
        SetWindowTextW(GetDlgItem(window_, 12), get_text(locale_, text::daily, text_en::daily)); // Daily button
        SetWindowTextW(GetDlgItem(window_, 13), get_text(locale_, text::duration_hint, text_en::duration_hint)); // Duration hint

        // Update buttons that depend on state
        SetWindowTextW(GetDlgItem(window_, 15), editing_id_ ? get_text(locale_, text::save_schedule, text_en::save_schedule) : get_text(locale_, text::add_schedule, text_en::add_schedule)); // Add schedule
        SetWindowTextW(GetDlgItem(window_, 16), get_text(locale_, text::new_schedule, text_en::new_schedule)); // New schedule
        SetWindowTextW(GetDlgItem(window_, 17), get_text(locale_, text::schedules, text_en::schedules)); // Schedules label
        SetWindowTextW(GetDlgItem(window_, 18), get_text(locale_, text::edit, text_en::edit)); // Edit button

        // Update pause button based on state
        if (selected_record() && selected_record()->paused) {
            SetWindowTextW(GetDlgItem(window_, 19), get_text(locale_, text::resume, text_en::resume));
        } else {
            SetWindowTextW(GetDlgItem(window_, 19), get_text(locale_, text::pause, text_en::pause));
        }

        SetWindowTextW(GetDlgItem(window_, 20), get_text(locale_, text::erase, text_en::erase)); // Erase button
        SetWindowTextW(GetDlgItem(window_, 21), get_text(locale_, text::cancel, text_en::cancel)); // Cancel button

        // Update status and other dynamic text (will be updated by refresh())
        if (selected_record()) {
            SetWindowTextW(GetDlgItem(window_, 22), state_label(*selected_record()));
        } else {
            SetWindowTextW(GetDlgItem(window_, 22), get_text(locale_, text::idle, text_en::idle));
        }

        // Update locale combo box items and selection
        if (locale_combo_) {
            SendMessageW(locale_combo_, CB_RESETCONTENT, 0, 0);
            SendMessageW(locale_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(get_text(locale_, text::portuguese, text_en::portuguese)));
            SendMessageW(locale_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(get_text(locale_, text::english, text_en::english)));
            SendMessageW(locale_combo_, CB_SETCURSEL, static_cast<WPARAM>(locale_), 0);
        }

        // Update theme combo box items and selection
        if (theme_combo_) {
            SendMessageW(theme_combo_, CB_RESETCONTENT, 0, 0);
            for (const auto* theme : text::theme_strings) SendMessageW(theme_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(theme));
            SendMessageW(theme_combo_, CB_SETCURSEL, static_cast<WPARAM>(theme_value_), 0);
        }

        // Update action combo box
        if (action_) {
            SendMessageW(action_, CB_RESETCONTENT, 0, 0);
            for (const auto* action : text::actions) SendMessageW(action_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(action));
            if (selected_record()) SendMessageW(action_, CB_SETCURSEL, static_cast<WPARAM>(selected_record()->action), 0);
            else SendMessageW(action_, CB_SETCURSEL, 0, 0);
        }

        // Update kind combo box
        if (kind_) {
            SendMessageW(kind_, CB_RESETCONTENT, 0, 0);
            for (const auto* kind : text::kinds) SendMessageW(kind_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kind));
            if (selected_record()) SendMessageW(kind_, CB_SETCURSEL, static_cast<WPARAM>(selected_record()->kind), 0);
            else SendMessageW(kind_, CB_SETCURSEL, 0, 0);
        }

        // Update preset buttons
        for (int i = 0; i < static_cast<int>(std::size(text::presets)); ++i) {
            HWND preset = GetDlgItem(window_, preset_id + i);
            if (preset) {
                SetWindowTextW(preset, text::presets[i].label);
            }
        }

        // Update day buttons
        for (int i = 0; i < 7; ++i) {
            HWND day = GetDlgItem(window_, weekday_id + i);
            if (day) {
                SetWindowTextW(day, text::weekdays[i]);
            }
        }

        // Update list column headers
        if (list_) {
            const wchar_t* labels[] = {get_text(locale_, text::id_column, text_en::id_column), get_text(locale_, text::action_column, text_en::action_column), get_text(locale_, text::due_column, text_en::due_column), get_text(locale_, text::state_column, text_en::state_column), get_text(locale_, text::rule_column, text_en::rule_column)};
            const int widths[] = {40, 80, 172, 130, 80};
            for (int i = 0; i < 5; ++i) {
                LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
                column.pszText = const_cast<wchar_t*>(labels[i]); column.cx = px(widths[i]);
                ListView_SetColumn(list_, i, &column);
            }
        }

        // Update other buttons
        if (open_id) {
            HWND open_btn = GetDlgItem(window_, open_id);
            if (open_btn) set_text(open_btn, get_text(locale_, text::open, text_en::open));
        }
        if (settings_id) {
            HWND settings_btn = GetDlgItem(window_, settings_id);
            if (settings_btn) set_text(settings_btn, get_text(locale_, text::settings, text_en::settings));
        }
        if (visibility_id) {
            HWND visibility_btn = GetDlgItem(window_, visibility_id);
            if (visibility_btn) set_text(visibility_btn, get_text(locale_, text::notification_settings, text_en::notification_settings));
        }
        if (menu_id) {
            HWND menu_btn = GetDlgItem(window_, menu_id);
            if (menu_btn) set_text(menu_btn, get_text(locale_, text::options, text_en::options));
        }
        if (exit_id) {
            HWND exit_btn = GetDlgItem(window_, exit_id);
            if (exit_btn) set_text(exit_btn, get_text(locale_, text::exit, text_en::exit));
        }

        // Refresh dynamic content
        preview();
        refresh();
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
        if (list_) {
            const int widths[] = {40, 80, 172, 130, 80};
            for (int i = 0; i < 5; ++i) ListView_SetColumnWidth(list_, i, px(widths[i]));
        }
    }
    // Get system theme preference: 0=dark, 1=light
    bool get_system_dark_theme() {
        HKEY hKey;
        DWORD value = 0;
        DWORD size = sizeof(DWORD);
        LONG result = RegOpenKeyExW(
            HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            0,
            KEY_READ,
            &hKey
        );
        if (result == ERROR_SUCCESS) {
            result = RegQueryValueExW(
                hKey,
                L"AppsUseLightTheme",
                nullptr,
                nullptr,
                reinterpret_cast<LPBYTE>(&value),
                &size
            );
            RegCloseKey(hKey);
            if (result == ERROR_SUCCESS) {
                return value == 0; // 0 = dark theme, 1 = light theme
            }
        }
        // Default to light if we can't read the registry
        return false;
    }

    void colors() {
        HIGHCONTRASTW contrast{sizeof(contrast), 0, nullptr};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        high_contrast_ = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;

        if (high_contrast_) {
            bg_ = GetSysColor(COLOR_WINDOW);
            ink_ = GetSysColor(COLOR_WINDOWTEXT);
            accent_ = GetSysColor(COLOR_HIGHLIGHT);
        } else {
            bool use_dark = false;
            switch (theme_value_) {
                case 0: // automatic
                    use_dark = get_system_dark_theme();
                    break;
                case 1: // light
                    use_dark = false;
                    break;
                case 2: // dark
                    use_dark = true;
                    break;
            }

            if (use_dark) {
                // Dark theme colors
                bg_ = RGB(30, 30, 30);
                ink_ = RGB(240, 240, 240);
                accent_ = RGB(0, 120, 212); // Keep our brand blue
            } else {
                // Light theme colors
                bg_ = RGB(247, 249, 252);
                ink_ = RGB(24, 35, 53);
                accent_ = RGB(0, 120, 212); // Our brand blue
            }
        }

        if (background_) DeleteObject(background_);
        background_ = CreateSolidBrush(bg_);
        if (window_) RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        if (list_) {
            ListView_SetTextColor(list_, ink_);
            ListView_SetTextBkColor(list_, bg_);
        }
    }
    void controls() {
        add(L"STATIC", get_text(locale_, text::brand, text_en::brand), 0, 0, 24, 20, 360, 36, 1);
        add(L"STATIC", get_text(locale_, text::simulation, text_en::simulation), SS_RIGHT, 0, 454, 28, 102, 20, 2);
        add(L"STATIC", get_text(locale_, text::subtitle, text_en::subtitle), 0, 0, 24, 60, 360, 22);
        add(L"BUTTON", get_text(locale_, text::options, text_en::options), WS_TABSTOP | BS_OWNERDRAW, menu_id, 484, 58, 72, 28, 2);

        // Theme selection near other combos for logical tab order
        add(L"STATIC", get_text(locale_, text::theme_label, text_en::theme_label), 0, 0, 24, 100, 254, 22);
        theme_combo_ = add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                     theme_id, 24, 125, 254, 180);
        for (const auto* theme : text::theme_strings) SendMessageW(theme_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(theme));
        SendMessageW(theme_combo_, CB_SETCURSEL, static_cast<WPARAM>(theme_value_), 0);

        // Language selection
        add(L"STATIC", get_text(locale_, text::language_label, text_en::language_label), 0, 0, 24, 170, 254, 22);
        locale_combo_ = add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                     locale_id, 24, 195, 254, 180);
        SendMessageW(locale_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(get_text(locale_, text::portuguese, text_en::portuguese)));

        // Kind selection: right column, same row as language
        add(L"STATIC", get_text(locale_, text::kind_label, text_en::kind_label), 0, 0, 304, 170, 252, 22);
        kind_ = add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                    kind_id, 304, 195, 252, 160);
        SendMessageW(locale_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(get_text(locale_, text::english, text_en::english)));
        SendMessageW(locale_combo_, CB_SETCURSEL, static_cast<WPARAM>(book_.get_locale()), 0);

        add(L"STATIC", get_text(locale_, text::action_label, text_en::action_label), 0, 0, 304, 100, 252, 22);
        action_ = add(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                      action_id, 304, 125, 252, 180);
        for (const auto* action : text::actions) SendMessageW(action_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(action));
        SendMessageW(action_, CB_SETCURSEL, 0, 0);
        for (const auto* kind : text::kinds) SendMessageW(kind_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kind));
        SendMessageW(kind_, CB_SETCURSEL, 0, 0);
        duration_controls_.push_back(add(L"STATIC", get_text(locale_, text::duration_label, text_en::duration_label), 0, 0, 24, 240, 254, 22));
        duration_ = add(L"EDIT", L"30", WS_TABSTOP | ES_AUTOHSCROLL, duration_id,
                        24, 265, 254, 34, 0, WS_EX_CLIENTEDGE);
        duration_controls_.push_back(duration_);
        SendMessageW(duration_, EM_SETLIMITTEXT, 32, 0);
        duration_controls_.push_back(add(L"STATIC", get_text(locale_, text::duration_hint, text_en::duration_hint), 0, 0, 304, 272, 252, 22, 2));
        for (int i = 0; i < static_cast<int>(std::size(text::presets)); ++i)
            duration_controls_.push_back(add(L"BUTTON", text::presets[i].label, WS_TABSTOP | BS_OWNERDRAW, preset_id + i,
                24 + 136 * i, 315, 124, 32, 2));
        once_controls_.push_back(add(L"STATIC", get_text(locale_, text::date_label, text_en::date_label), 0, 0, 24, 240, 254, 22));
        date_ = add(DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_SHORTDATEFORMAT, date_id, 24, 265, 254, 34);
        SendMessageW(date_, DTM_SETFORMATW, 0, reinterpret_cast<LPARAM>(L"dd/MM/yyyy"));
        SYSTEMTIME range[2]{}; range[0].wYear = 1970; range[0].wMonth = 1; range[0].wDay = 1;
        range[1].wYear = 9999; range[1].wMonth = 12; range[1].wDay = 31;
        DateTime_SetRange(date_, GDTR_MIN | GDTR_MAX, range);
        once_controls_.push_back(date_);
        calendar_controls_.push_back(add(L"STATIC", get_text(locale_, text::time_label, text_en::time_label), 0, 0, 304, 240, 252, 22));
        time_ = add(DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_TIMEFORMAT | DTS_UPDOWN, time_id, 304, 265, 252, 34);
        SendMessageW(time_, DTM_SETFORMATW, 0, reinterpret_cast<LPARAM>(L"HH:mm"));
        calendar_controls_.push_back(time_);
        weekly_controls_.push_back(add(L"STATIC", get_text(locale_, text::weekly_label, text_en::weekly_label), 0, 0, 24, 240, 254, 22, 2));
        weekly_controls_.push_back(add(L"BUTTON", get_text(locale_, text::daily, text_en::daily), WS_TABSTOP | BS_OWNERDRAW, daily_id, 24, 265, 254, 34, 2));
        for (int i = 0; i < 7; ++i) {
            days_[i] = add(L"BUTTON", text::weekdays[i], WS_TABSTOP | BS_AUTOCHECKBOX, weekday_id + i,
                           24 + 76 * i, 315, 74, 30, 2);
            SendMessageW(days_[i], BM_SETCHECK, BST_CHECKED, 0);
            weekly_controls_.push_back(days_[i]);
        }
        preview_ = add(L"STATIC", L"", 0, 0, 24, 360, 532, 30, 2);
        start_ = add(L"BUTTON", get_text(locale_, text::add_schedule, text_en::add_schedule), WS_TABSTOP | BS_OWNERDRAW, schedule_id,
                     24, 400, 344, 36);
        add(L"BUTTON", get_text(locale_, text::new_schedule, text_en::new_schedule), WS_TABSTOP | BS_OWNERDRAW, new_id, 380, 400, 176, 36);
        add(L"STATIC", get_text(locale_, text::schedules, text_en::schedules), 0, 0, 24, 450, 532, 22);
        list_ = add(WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                    list_id, 24, 476, 532, 120, 2, WS_EX_CLIENTEDGE);
        ListView_SetExtendedListViewStyle(list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        const wchar_t* labels[] = {get_text(locale_, text::id_column, text_en::id_column), get_text(locale_, text::action_column, text_en::action_column), get_text(locale_, text::due_column, text_en::due_column), get_text(locale_, text::state_column, text_en::state_column), get_text(locale_, text::rule_column, text_en::rule_column)};
        const int widths[] = {40, 80, 172, 130, 80};
        for (int i = 0; i < 5; ++i) {
            LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(labels[i]); column.cx = px(widths[i]);
            ListView_InsertColumn(list_, i, &column);
        }
        edit_ = add(L"BUTTON", get_text(locale_, text::edit, text_en::edit), WS_TABSTOP | BS_OWNERDRAW, edit_id, 24, 608, 124, 36, 2);
        pause_ = add(L"BUTTON", get_text(locale_, text::pause, text_en::pause), WS_TABSTOP | BS_OWNERDRAW, pause_id, 160, 608, 124, 36, 2);
        erase_ = add(L"BUTTON", get_text(locale_, text::erase, text_en::erase), WS_TABSTOP | BS_OWNERDRAW, erase_id, 296, 608, 124, 36, 2);
        cancel_ = add(L"BUTTON", get_text(locale_, text::cancel, text_en::cancel), WS_TABSTOP | BS_OWNERDRAW, cancel_id, 432, 608, 124, 36, 2);
        status_ = add(L"STATIC", get_text(locale_, text::idle, text_en::idle), SS_CENTER, 0, 24, 660, 532, 24);
        countdown_ = add(L"STATIC", get_text(locale_, text::clock_empty, text_en::clock_empty), SS_CENTER, 0, 24, 688, 532, 56, 3);
        detail_ = add(L"STATIC", get_text(locale_, text::idle_detail, text_en::idle_detail), SS_CENTER, 0, 24, 748, 532, 40, 2);
        error_ = add(L"STATIC", L"", SS_CENTER, 0, 24, 792, 532, 42, 2);
        add(L"STATIC", get_text(locale_, text::simulation_note, text_en::simulation_note), SS_CENTER, 0, 24, 842, 532, 22, 2);
        add(L"STATIC", get_text(locale_, text::settings_note, text_en::settings_note), SS_CENTER, 0, 24, 872, 532, 22, 2);
        fonts_and_layout();
        show_kind();
    }
    std::wstring duration_text() const {
        wchar_t value[64]{};
        GetWindowTextW(duration_, value, 64);
        return value;
    }
    Action selected_action() const {
        return static_cast<Action>(SendMessageW(action_, CB_GETCURSEL, 0, 0));
    }
    ScheduleKind selected_kind() const {
        return static_cast<ScheduleKind>(SendMessageW(kind_, CB_GETCURSEL, 0, 0));
    }
    CivilTime selected_civil() const {
        SYSTEMTIME date{}, time{};
        if (DateTime_GetSystemtime(date_, &date) != GDT_VALID ||
            DateTime_GetSystemtime(time_, &time) != GDT_VALID) return {};
        return {date.wYear, date.wMonth, date.wDay, time.wHour, time.wMinute};
    }
    unsigned selected_days() const {
        unsigned mask = 0;
        for (unsigned i = 0; i < 7; ++i)
            if (SendMessageW(days_[i], BM_GETCHECK, 0, 0) == BST_CHECKED) mask |= 1u << i;
        return mask;
    }
    void show_kind() {
        const auto kind = selected_kind();
        for (const auto control : duration_controls_) ShowWindow(control, kind == ScheduleKind::Duration ? SW_SHOW : SW_HIDE);
        for (const auto control : once_controls_) ShowWindow(control, kind == ScheduleKind::Once ? SW_SHOW : SW_HIDE);
        for (const auto control : weekly_controls_) ShowWindow(control, kind == ScheduleKind::Weekly ? SW_SHOW : SW_HIDE);
        for (const auto control : calendar_controls_) ShowWindow(control, kind != ScheduleKind::Duration ? SW_SHOW : SW_HIDE);
        preview();
    }
    void preview() {
        if (!preview_) return;
        const auto now = clock_sample();
        const auto kind = selected_kind();
        const auto civil = selected_civil();
        std::optional<std::uint64_t> due;
        if (kind == ScheduleKind::Duration) {
            const auto minutes = Scheduler::parse_duration(duration_text());
            if (minutes && now.utc_ms <= UINT64_MAX - *minutes * 60000) due = now.utc_ms + *minutes * 60000;
        } else if (kind == ScheduleKind::Once) due = book_.preview_once(civil, now);
        else if (kind == ScheduleKind::Weekly) due = book_.preview_weekly(selected_days(), civil.hour * 60 + civil.minute, now);
        const auto action = static_cast<int>(selected_action());
        if (!due || action < 0 || action > 2) {
            set_text(preview_, kind == ScheduleKind::Duration ? get_text(locale_, text::invalid_preview, text_en::invalid_preview) : get_text(locale_, text::calendar_preview_error, text_en::calendar_preview_error));
            return;
        }
        set_text(preview_, std::wstring(get_text(locale_, text::preview_prefix, text_en::preview_prefix)) + get_text(locale_, text::actions[action], text_en::actions[action]) +
            get_text(locale_, text::separator, text_en::separator) + local_time_at(*due, locale_));
    }
    void notify(const wchar_t* title, const wchar_t* body) {
        if (!tray_added_) return;
        NOTIFYICONDATAW notice = tray_;
        notice.uFlags = NIF_INFO;
        wcsncpy_s(notice.szInfoTitle, title, _TRUNCATE);
        wcsncpy_s(notice.szInfo, body, _TRUNCATE);
        // Respect sound setting: NIIF_NOSOUND only if sound disabled
        UINT sound_flag = book_.get_theme() & 1 ? 0 : NIIF_NOSOUND;
        notice.dwInfoFlags = NIIF_INFO | sound_flag | NIIF_RESPECT_QUIET_TIME;
        Shell_NotifyIconW(NIM_MODIFY, &notice);

        // Play system beep if sound enabled (fallback to MessageBeep if PlaySound fails)
        if (book_.get_sound_enabled()) {
            MessageBeep(MB_OK); // Simple system beep
        }
    }
    const ScheduleRecord* selected_record() const {
        for (const auto& record : book_.list()) if (record.id == selected_id_) return &record;
        return nullptr;
    }
    const wchar_t* state_label(const ScheduleRecord& record) {
        if (record.conflict_with) return get_text(locale_, text::state_conflict, text_en::state_conflict);
        if (record.state == OccurrenceState::Scheduled && record.paused) return get_text(locale_, text::state_paused, text_en::state_paused);
        switch (record.state) {
            case OccurrenceState::Scheduled: return get_text(locale_, text::state_scheduled, text_en::state_scheduled);
            case OccurrenceState::Cancelled: return get_text(locale_, text::state_cancelled, text_en::state_cancelled);
            case OccurrenceState::Expired: return get_text(locale_, text::state_expired, text_en::state_expired);
            case OccurrenceState::Uncertain: return get_text(locale_, text::state_uncertain, text_en::state_uncertain);
            case OccurrenceState::Completed: return get_text(locale_, text::state_completed, text_en::state_completed);
        }
        return get_text(locale_, text::state_uncertain, text_en::state_uncertain);
    }
    void show_error() {
        if (book_.error() == BookError::Conflict) {
            std::wstring message = std::wstring(get_text(locale_, text::conflict_error, text_en::conflict_error)) + text::schedule_number + std::to_wstring(book_.conflicting_id());
            for (const auto& record : book_.list()) if (record.id == book_.conflicting_id()) {
                message += std::wstring(get_text(locale_, text::separator, text_en::separator)) + get_text(locale_, text::actions[static_cast<int>(record.action)], text_en::actions[static_cast<int>(record.action)]) +
                    std::wstring(get_text(locale_, text::separator, text_en::separator)) + local_time_at(record.due_utc_ms, locale_);
                break;
            }
            set_text(error_, message);
            return;
        }
        const wchar_t* message = L"";
        switch (book_.error()) {
            case BookError::InvalidInput: message = selected_kind() == ScheduleKind::Duration ? get_text(locale_, text::record_input_error, text_en::record_input_error) : get_text(locale_, text::calendar_input_error, text_en::calendar_input_error); break;
            case BookError::Calendar: message = get_text(locale_, text::calendar_error, text_en::calendar_error); break;
            case BookError::NotFound: message = get_text(locale_, text::missing_record, text_en::missing_record); break;
            case BookError::Storage: message = get_text(locale_, text::storage_error, text_en::storage_error); break;
            case BookError::InvalidData: message = get_text(locale_, text::invalid_data, text_en::invalid_data); break;
            case BookError::NotReady: message = get_text(locale_, text::not_ready, text_en::not_ready); break;
            case BookError::None: break;
            case BookError::Conflict: break;
        }
        set_text(error_, message);
    }
    void sync_list() {
        std::wstring signature;
        for (const auto& record : book_.list()) {
            signature += std::to_wstring(record.id) + L":" + std::to_wstring(record.occurrence_id) + L":" +
                std::to_wstring(record.due_utc_ms) + L":" + std::to_wstring(static_cast<int>(record.state)) +
                (record.paused ? L"p:" : L"a:") + std::to_wstring(record.conflict_with) + L";";
        }
        if (signature == list_signature_) return;
        list_signature_ = signature;
        if (!selected_record()) selected_id_ = book_.list().empty() ? 0 : book_.list().front().id;
        rebuilding_list_ = true;
        SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
        ListView_DeleteAllItems(list_);
        displayed_ids_.clear();
        for (const auto& record : book_.list()) {
            const int row = static_cast<int>(displayed_ids_.size());
            displayed_ids_.push_back(record.id);
            LVITEMW item{}; item.mask = LVIF_TEXT; item.iItem = row;
            auto number = std::to_wstring(record.id);
            item.pszText = number.data();
            ListView_InsertItem(list_, &item);
            auto due = record.state == OccurrenceState::Scheduled ? local_time_at(record.due_utc_ms, locale_) : std::wstring(get_text(locale_, text::no_next, text_en::no_next));
            auto rule = record.kind == ScheduleKind::Duration ? std::to_wstring(record.minutes) + L" min" :
                std::wstring(record.kind == ScheduleKind::Once ? text::once_rule : text::weekly_rule);
            ListView_SetItemText(list_, row, 1, const_cast<wchar_t*>(get_text(locale_, text::actions[static_cast<int>(record.action)], text_en::actions[static_cast<int>(record.action)])));
            ListView_SetItemText(list_, row, 2, due.data());
            ListView_SetItemText(list_, row, 3, const_cast<wchar_t*>(state_label(record)));
            ListView_SetItemText(list_, row, 4, rule.data());
            if (record.id == selected_id_) ListView_SetItemState(list_, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
        SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list_, nullptr, TRUE);
        rebuilding_list_ = false;
    }
    void refresh() {
        if (!countdown_) return;
        const auto now = clock_sample();
        for (const auto& delivery : book_.tick(now)) {
            if (log_message) log_message(get_text(locale_, text::completed_detail, text_en::completed_detail));
            const bool recorded = book_.complete(delivery);
            if (!recorded) show_error();
            notify(recorded ? get_text(locale_, text::completed, text_en::completed) : get_text(locale_, text::state_uncertain, text_en::state_uncertain),
                recorded ? get_text(locale_, text::completed_detail, text_en::completed_detail) : get_text(locale_, text::uncertain_detail, text_en::uncertain_detail));
            if (!recorded) break;
        }
        // Multi-threshold warnings: 30min, 15min, 10min, 5min, 1min (existing warning_ms)
        // Check from largest to smallest threshold to ensure we fire the highest applicable threshold first
        const std::array<std::pair<std::uint64_t, size_t>, 5> thresholds = {{
            {30ULL * 60 * 1000, 0},  // 30 minutes
            {15ULL * 60 * 1000, 1},  // 15 minutes
            {10ULL * 60 * 1000, 2},  // 10 minutes
            {5ULL * 60 * 1000, 3},   // 5 minutes
            {Scheduler::warning_ms, 4} // 1 minute (existing)
        }};

        for (const auto& record : book_.list()) {
            if (!book_.writable()) break;
            if (record.state != OccurrenceState::Scheduled || record.paused || record.conflict_with) continue;

            const auto remaining = book_.remaining_ms(record.id, now);
            if (remaining == 0) continue; // Skip expired

            // Thresholds already crossed the first time this occurrence is seen (e.g. a
            // 5 minute timer) were never "reached" by the countdown: mark them without notifying.
            const auto seen_key = record.occurrence_id * 10 + 9;
            const bool first_sight = warned_thresholds_.insert(seen_key).second;
            // Only the tightest crossed threshold applies; wider ones are marked silently.
            int applicable = -1;
            for (int i = 0; i < static_cast<int>(thresholds.size()); ++i)
                if (remaining <= thresholds[i].first) applicable = i;
            if (applicable < 0) continue;
            for (int i = 0; i < applicable; ++i)
                warned_thresholds_.insert(record.occurrence_id * 10 + thresholds[i].second);
            if (first_sight) {
                warned_thresholds_.insert(record.occurrence_id * 10 + thresholds[applicable].second);
                continue;
            }
            {
                const auto threshold_index = thresholds[applicable].second;
                {
                    const auto warn_key = record.occurrence_id * 10 + threshold_index;
                    if (warned_thresholds_.insert(warn_key).second) {
                        // Select appropriate message based on threshold
                        const wchar_t* title = get_text(locale_, text::warning, text_en::warning);
                        const wchar_t* body = get_text(locale_, text::countdown_notification, text_en::countdown_notification); // fallback

                        switch (threshold_index) {
                            case 0: // 30 minutes
                                title = get_text(locale_, text::warning_30min, text_en::warning_30min);
                                body = get_text(locale_, text::warning_30min_detail, text_en::warning_30min_detail);
                                break;
                            case 1: // 15 minutes
                                title = get_text(locale_, text::warning_15min, text_en::warning_15min);
                                body = get_text(locale_, text::warning_15min_detail, text_en::warning_15min_detail);
                                break;
                            case 2: // 10 minutes
                                title = get_text(locale_, text::warning_10min, text_en::warning_10min);
                                body = get_text(locale_, text::warning_10min_detail, text_en::warning_10min_detail);
                                break;
                            case 3: // 5 minutes
                                title = get_text(locale_, text::warning_5min, text_en::warning_5min);
                                body = get_text(locale_, text::warning_5min_detail, text_en::warning_5min_detail);
                                break;
                            case 4: // 1 minute (existing)
                                title = get_text(locale_, text::warning, text_en::warning);
                                body = get_text(locale_, text::countdown_notification, text_en::countdown_notification);
                                break;
                        }

                        notify(title, body);
                        // Don't show() here - only show for the 1-minute warning to avoid too many popups
                        if (threshold_index == 4) { // 1-minute warning
                            show(false);
                        }
                    }
                }
            }
        }
        std::wstring conflicts;
        for (const auto& record : book_.list()) if (record.conflict_with)
            conflicts += std::to_wstring(record.id) + L":" + std::to_wstring(record.conflict_with) + L";";
        if (conflicts != conflict_signature_) {
            conflict_signature_ = conflicts;
            if (!conflicts.empty()) notify(get_text(locale_, text::state_conflict, text_en::state_conflict), get_text(locale_, text::conflict_notification, text_en::conflict_notification));
        }
        sync_list();
        const auto* record = selected_record();
        const bool writable = book_.writable();
        const bool scheduled = record && record->state == OccurrenceState::Scheduled;
        EnableWindow(start_, writable);
        EnableWindow(edit_, writable && record);
        EnableWindow(erase_, writable && record);
        EnableWindow(pause_, writable && scheduled);
        EnableWindow(cancel_, writable && scheduled);
        set_text(pause_, record && record->paused ? get_text(locale_, text::resume, text_en::resume) : get_text(locale_, text::pause, text_en::pause));
        set_text(start_, editing_id_ ? get_text(locale_, text::save_schedule, text_en::save_schedule) : get_text(locale_, text::add_schedule, text_en::add_schedule));
        if (record) {
            set_text(status_, state_label(*record));
            if (scheduled && !record->paused && !record->conflict_with) {
                const auto seconds = (book_.remaining_ms(record->id, now) + 999) / 1000;
                wchar_t value[48]{};
                swprintf_s(value, L"%02llu:%02llu:%02llu", seconds / 3600, (seconds / 60) % 60, seconds % 60);
                set_text(countdown_, value);
            } else set_text(countdown_, get_text(locale_, text::clock_empty, text_en::clock_empty));
            const wchar_t* note = record->state == OccurrenceState::Uncertain ? get_text(locale_, text::uncertain_detail, text_en::uncertain_detail) :
                record->paused && scheduled ? get_text(locale_, text::paused_detail, text_en::paused_detail) : nullptr;
            std::wstring detail = note ? std::wstring(note) : std::wstring(get_text(locale_, text::actions[static_cast<int>(record->action)], text_en::actions[static_cast<int>(record->action)])) +
                get_text(locale_, text::separator, text_en::separator) + local_time_at(record->due_utc_ms, locale_);
            if (record->conflict_with) detail = std::wstring(get_text(locale_, text::conflict_detail, text_en::conflict_detail)) +
                std::to_wstring(record->conflict_with) + get_text(locale_, text::conflict_hint, text_en::conflict_hint);
            else if (record->kind == ScheduleKind::Weekly) {
                detail += L"\n";
                for (unsigned i = 0; i < 7; ++i) if (record->weekdays & (1u << i)) detail += std::wstring(get_text(locale_, text::weekdays[i], text_en::weekdays[i])) + L" ";
                if (record->last_occurrence_id) {
                    auto last = *record; last.state = record->last_state; last.paused = false; last.conflict_with = 0;
                    detail += std::wstring(get_text(locale_, text::separator, text_en::separator)) + get_text(locale_, text::last_result, text_en::last_result) + state_label(last);
                }
            }
            set_text(detail_, detail);
        } else {
            set_text(status_, get_text(locale_, text::idle, text_en::idle)); set_text(countdown_, get_text(locale_, text::clock_empty, text_en::clock_empty)); set_text(detail_, get_text(locale_, text::idle_detail, text_en::idle_detail));
        }
        if (!writable) show_error();
        preview();
        if (!tray_added_ && now.monotonic_ms - last_tray_try_ >= 5000) add_tray();
    }
    void command(int id) {
        if (id >= preset_id && id < preset_id + static_cast<int>(std::size(text::presets))) {
            const auto minutes = std::to_wstring(text::presets[id - preset_id].minutes);
            SetWindowTextW(duration_, minutes.c_str()); SetFocus(duration_); SendMessageW(duration_, EM_SETSEL, 0, -1);
        } else if (id == schedule_id) {
            const bool editing = editing_id_ != 0;
            const auto now = clock_sample();
            const auto action = selected_action();
            const auto civil = selected_civil();
            bool ok = false;
            switch (selected_kind()) {
                case ScheduleKind::Duration: ok = editing ? book_.edit(editing_id_, action, duration_text(), now) :
                    book_.create(action, duration_text(), now); break;
                case ScheduleKind::Once: ok = editing ? book_.edit_once(editing_id_, action, civil, now) :
                    book_.create_once(action, civil, now); break;
                case ScheduleKind::Weekly: ok = editing ? book_.edit_weekly(editing_id_, action, selected_days(), civil.hour * 60 + civil.minute, now) :
                    book_.create_weekly(action, selected_days(), civil.hour * 60 + civil.minute, now); break;
            }
            if (ok) {
                selected_id_ = editing ? editing_id_ : book_.list().back().id;
                editing_id_ = 0;
            }
            show_error(); refresh();
        } else if (id == new_id) {
            editing_id_ = 0; set_text(error_, L""); refresh();
            SetFocus(selected_kind() == ScheduleKind::Duration ? duration_ : time_);
        } else if (id == edit_id) {
            if (const auto* record = selected_record()) {
                editing_id_ = record->id;
                SendMessageW(action_, CB_SETCURSEL, static_cast<WPARAM>(record->action), 0);
                SendMessageW(kind_, CB_SETCURSEL, static_cast<WPARAM>(record->kind), 0);
                set_text(duration_, std::to_wstring(record->minutes));
                if (record->kind != ScheduleKind::Duration) {
                    const auto civil = record->kind == ScheduleKind::Once ? calendar_.local(record->due_utc_ms).value_or(record->civil) : record->civil;
                    SYSTEMTIME selected{};
                    selected.wYear = static_cast<WORD>(civil.year); selected.wMonth = static_cast<WORD>(civil.month);
                    selected.wDay = static_cast<WORD>(civil.day); selected.wHour = static_cast<WORD>(civil.hour); selected.wMinute = static_cast<WORD>(civil.minute);
                    DateTime_SetSystemtime(date_, GDT_VALID, &selected); DateTime_SetSystemtime(time_, GDT_VALID, &selected);
                    for (unsigned i = 0; i < 7; ++i) SendMessageW(days_[i], BM_SETCHECK, record->weekdays & (1u << i) ? BST_CHECKED : BST_UNCHECKED, 0);
                }
                const auto focus = record->kind == ScheduleKind::Duration ? duration_ : time_;
                show_kind(); refresh(); SetFocus(focus);
            }
        } else if (id == pause_id || id == erase_id || id == cancel_id) {
            if (const auto* record = selected_record()) {
                const auto selected = record->id;
                if (id == pause_id) book_.set_paused(selected, !record->paused, clock_sample());
                else if (id == erase_id) {
                    if (book_.erase(selected) && editing_id_ == selected) editing_id_ = 0;
                } else book_.cancel(selected);
                show_error(); refresh();
            }
        } else if (id == daily_id) {
            for (auto day : days_) SendMessageW(day, BM_SETCHECK, BST_CHECKED, 0);
            preview();
        } else if (id >= weekday_id && id < weekday_id + 7) {
            preview();
        } else if (id == open_id || id == settings_id) {
            show(true);
            if (id == settings_id) set_text(error_, get_text(locale_, text::settings_note, text_en::settings_note));
        } else if (id == visibility_id) {
            const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(window_, L"open",
                L"ms-settings:taskbar", nullptr, nullptr, SW_SHOWNORMAL));
            if (result <= 32) { show(true); set_text(error_, get_text(locale_, text::settings_error, text_en::settings_error)); }
        } else if (id == menu_id) {
            menu();
        } else if (id == exit_id) {
            user_requested_exit.store(true); SetEvent(stop_event); DestroyWindow(window_);
        }
    }
    void menu() {
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        AppendMenuW(menu, MF_STRING, open_id, get_text(locale_, text::open, text_en::open));
        AppendMenuW(menu, MF_STRING, settings_id, get_text(locale_, text::settings, text_en::settings));
        AppendMenuW(menu, MF_STRING, visibility_id, get_text(locale_, text::notification_settings, text_en::notification_settings));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (book_.writable() && selected_record() && selected_record()->state == OccurrenceState::Scheduled ? MF_ENABLED : MF_GRAYED), cancel_id, get_text(locale_, text::cancel, text_en::cancel));
        AppendMenuW(menu, MF_STRING, exit_id, get_text(locale_, text::exit, text_en::exit));
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

        // Check if theme is dark
        bool use_dark = false;
        switch (theme_value_) {
            case 0: use_dark = get_system_dark_theme(); break;
            case 1: use_dark = false; break;
            case 2: use_dark = true; break;
        }

        const COLORREF fill = primary && !disabled ? accent_ : high_contrast_ ? GetSysColor(COLOR_BTNFACE) :
            use_dark ? RGB(60, 60, 60) : RGB(233, 239, 248);
        const COLORREF foreground = disabled ? GetSysColor(COLOR_GRAYTEXT) : primary ?
            (high_contrast_ ? GetSysColor(COLOR_BTNTEXT) : RGB(255, 255, 255)) :
            use_dark ? RGB(240, 240, 240) : RGB(24, 35, 53);

        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, fill);
        const auto old_brush = SelectObject(draw.hDC, brush);
        const auto old_pen = SelectObject(draw.hDC, pen);
        RoundRect(draw.hDC, draw.rcItem.left, draw.rcItem.top, draw.rcItem.right, draw.rcItem.bottom, px(10), px(10));
        SelectObject(draw.hDC, old_brush); SelectObject(draw.hDC, old_pen);
        DeleteObject(brush); DeleteObject(pen);

        // Find font for this button
        int font_index = 2; // default font
        for (const auto& item : items_) {
            if (item.handle == draw.hwndItem) {
                font_index = item.font;
                break;
            }
        }
        HFONT old_font = nullptr;
        if (fonts_[font_index]) {
            old_font = static_cast<HFONT>(SelectObject(draw.hDC, fonts_[font_index]));
        }

        wchar_t label[128]{};
        GetWindowTextW(draw.hwndItem, label, 128);
        SetBkMode(draw.hDC, TRANSPARENT);
        SetTextColor(draw.hDC, foreground);
        RECT rect = draw.rcItem;
        if (draw.itemState & ODS_SELECTED) OffsetRect(&rect, 0, 1);
        DrawTextW(draw.hDC, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (draw.itemState & ODS_FOCUS) { InflateRect(&rect, -px(4), -px(4)); DrawFocusRect(draw.hDC, &rect); }

        if (old_font) {
            SelectObject(draw.hDC, old_font);
        }
    }
public:
    Panel() : storage_(data_file()), book_(storage_, calendar_) {
        // Load theme and locale preference from storage
        auto loaded = storage_.load();
        if (loaded.status == power_timer::LoadStatus::Loaded) {
            theme_value_ = loaded.snapshot.theme;
            locale_ = loaded.snapshot.locale;
            book_.set_locale(locale_);
        }
        // Apply theme immediately
        colors();
        refresh_locale_text();
    }
    ~Panel() {
        if (window_ && IsWindow(window_)) DestroyWindow(window_);
        for (auto font : fonts_) if (font) DeleteObject(font);
        if (background_) DeleteObject(background_);
        if (icon_) DestroyIcon(icon_);
    }
    void show(bool activate) {
        ShowWindow(window_, activate ? SW_RESTORE : SW_SHOWNOACTIVATE);
        if (activate) { SetForegroundWindow(window_); SetFocus(selected_kind() == ScheduleKind::Duration ? duration_ : time_); }
    }
    void add_tray() {
        last_tray_try_ = GetTickCount64();
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_) != FALSE;
        if (tray_added_) {
            tray_.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &tray_);
            if (error_) set_text(error_, L"");
        } else if (error_) { set_text(error_, get_text(locale_, text::tray_error, text_en::tray_error)); show(false); }
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
        // Position window on the monitor where the tray icon / cursor is
        POINT cursor{};
        GetCursorPos(&cursor);
        HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        GetMonitorInfoW(monitor, &mi);
        work = mi.rcWork;
        RECT size{0, 0, px(580), px(920)};
        constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRectExForDpi(&size, style, FALSE, WS_EX_CONTROLPARENT, dpi_);
        const int width = size.right - size.left, height = size.bottom - size.top;
        window_ = CreateWindowExW(WS_EX_CONTROLPARENT, window_class, get_text(locale_, text::title, text_en::title), style,
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
        book_.initialize(clock_sample());
        try { controls(); } catch (...) { DestroyWindow(window_); return false; }
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = window_;
        tray_.uID = 1;
        tray_.uCallbackMessage = tray_message;
        tray_.hIcon = icon_;
        wcsncpy_s(tray_.szTip, get_text(locale_, text::title, text_en::title), _TRUNCATE);
        if (!SetTimer(window_, timer_id, 250, nullptr)) { DestroyWindow(window_); return false; }
        add_tray();
        // Initialize theme combo selection after controls created
        if (theme_combo_) SendMessageW(theme_combo_, CB_SETCURSEL, static_cast<WPARAM>(theme_value_), 0);
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
                }
                else if (LOWORD(wparam) == action_id && HIWORD(wparam) == CBN_SELCHANGE) self->preview();
                else if (LOWORD(wparam) == kind_id && HIWORD(wparam) == CBN_SELCHANGE) self->show_kind();
                else if (LOWORD(wparam) == theme_id && HIWORD(wparam) == CBN_SELCHANGE) {
                    self->theme_value_ = static_cast<unsigned>(SendMessageW(self->theme_combo_, CB_GETCURSEL, 0, 0));
                    self->book_.set_theme(self->theme_value_);
                    self->colors();
                    self->refresh_locale_text();
                    return 0;
                }
                else if (LOWORD(wparam) == locale_id && HIWORD(wparam) == CBN_SELCHANGE) {
                    unsigned locale = static_cast<unsigned>(SendMessageW(self->locale_combo_, CB_GETCURSEL, 0, 0));
                    self->book_.set_locale(locale);
                    self->locale_ = locale;
                    self->refresh_locale_text();
                    return 0;
                }
                else if (HIWORD(wparam) == BN_CLICKED) self->command(LOWORD(wparam));
                return 0;
            case WM_NOTIFY: {
                const auto* header = reinterpret_cast<NMHDR*>(lparam);
                if ((header->idFrom == date_id || header->idFrom == time_id) && header->code == DTN_DATETIMECHANGE) self->preview();
                if (header->idFrom == list_id && header->code == LVN_ITEMCHANGED && !self->rebuilding_list_) {
                    const int row = ListView_GetNextItem(self->list_, -1, LVNI_SELECTED);
                    if (row >= 0 && static_cast<size_t>(row) < self->displayed_ids_.size()) {
                        const auto selected = self->displayed_ids_[row];
                        if (selected != self->selected_id_) self->editing_id_ = 0;
                        self->selected_id_ = selected;
                        self->refresh();
                    }
                }
                return 0;
            }
            case WM_TIMER: if (wparam == timer_id) self->refresh(); return 0;
            case WM_POWERBROADCAST:
                if (wparam == PBT_APMRESUMEAUTOMATIC || wparam == PBT_APMRESUMESUSPEND) {
                    self->list_signature_.clear();
                    self->book_.reconcile(clock_sample()); self->refresh();
                }
                return TRUE;
            case WM_TIMECHANGE:
                self->list_signature_.clear();
                self->book_.reconcile(clock_sample()); self->refresh(); return 0;
            case tray_message:
                switch (LOWORD(lparam)) {
                    case NIN_SELECT: case NIN_KEYSELECT: case NIN_BALLOONUSERCLICK: self->show(true); break;
                    case WM_CONTEXTMENU: self->menu(); break;
                }
                return 0;
            case show_message: self->show(true); return 0;
            case sound_message:
                if (self->book_.writable() && !self->book_.set_sound_enabled(wparam != 0)) self->show_error();
                return 0;
            case WM_CONTEXTMENU: self->menu(); return 0;
            case stop_message: EndMenu(); DestroyWindow(window); return 0;
            case WM_CLOSE: ShowWindow(window, SW_HIDE); return 0;
            case WM_DESTROY:
                KillTimer(window, timer_id);
                self->book_.reconcile(clock_sample());
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
                    rect->bottom - rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
                // Ensure window stays on the correct monitor after DPI change
                POINT center{rect->left + (rect->right - rect->left) / 2, rect->top + (rect->bottom - rect->top) / 2};
                HMONITOR monitor = MonitorFromPoint(center, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (GetMonitorInfoW(monitor, &mi)) {
                    // Check if window is outside monitor work area
                    if (rect->left < mi.rcWork.left || rect->top < mi.rcWork.top ||
                        rect->right > mi.rcWork.right || rect->bottom > mi.rcWork.bottom) {
                        int width = rect->right - rect->left;
                        int height = rect->bottom - rect->top;
                        int x = mi.rcWork.right - width - self->px(20);
                        int y = (std::max)(mi.rcWork.top, mi.rcWork.bottom - height - self->px(20));
                        SetWindowPos(window, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                }
                return 0;
            }
            case WM_SETTINGCHANGE:
                self->colors();
                if (lparam && (lstrcmpiW(reinterpret_cast<LPCWSTR>(lparam), L"TimeZoneInformation") == 0)) {
                    self->list_signature_.clear();
                    self->book_.reconcile(clock_sample()); self->refresh();
                }
                return 0;
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
    INITCOMMONCONTROLSEX common{sizeof(common), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES};
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
                if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN && window) {
                    HWND focus = GetFocus();
                    // ENTER on duration edit triggers schedule
                    if (focus == GetDlgItem(window, duration_id)) {
                        PostMessageW(window, WM_COMMAND, schedule_id, 0); continue;
                    }
                    // ENTER on list view triggers edit
                    if (focus == GetDlgItem(window, list_id)) {
                        PostMessageW(window, WM_COMMAND, edit_id, 0); continue;
                    }
                    // ENTER on combos confirms selection (standard behavior)
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