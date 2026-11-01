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
#include <cwchar>
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
#include "../warning_tracker.hpp"
#include "../power_executor.hpp"
#include "windows_power_backend.hpp"

namespace power_timer::windows {
// The mod performs real power actions. Only the local preview executable runs in
// simulation mode, which swaps the wording below and uses SimulatedBackend.
inline bool simulation_mode = false;
struct SimulationText { const wchar_t* base_pt; const wchar_t* pt; const wchar_t* en; };
inline const SimulationText simulation_texts[] = {
    {text::title, L"Crazy Time · Simulação", L"Crazy Time · Simulation"},
    {text::simulation_note, L"Seu computador não será desligado nesta versão.", L"Your computer will not be shut down in this version."},
    {text::settings_note, L"SIMULAÇÃO · Duração e ação são configuradas neste painel.", L"SIMULATION · Duration and action are configured in this panel."},
    {text::schedule, L"Iniciar simulação", L"Start simulation"},
    {text::active, L"Simulação agendada", L"Simulation scheduled"},
    {text::completed, L"Simulação concluída", L"Simulation completed"},
    {text::completed_detail, L"Intenção registrada. Nenhuma ação real executada.", L"Intent recorded. No actual action executed."},
    {text::cancelled_detail, L"Você pode programar uma nova simulação.", L"You can schedule a new simulation."},
};
inline const wchar_t* get_text(unsigned locale, const wchar_t* pt_br, const wchar_t* en) {
    if (simulation_mode)
        for (const auto& item : simulation_texts)
            if (wcscmp(pt_br, item.base_pt) == 0) return locale == 1 ? item.en : item.pt;
    return locale == 1 ? en : pt_br;
}
inline constexpr wchar_t window_class[] = L"CrazyTime.Panel";
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
inline size_t (*mod_storage_path)(wchar_t*, size_t) = nullptr;
inline bool (*notification_sound_setting)() = nullptr;

enum ControlId { action_id = 100, duration_id, schedule_id, cancel_id,
                 preset_id = 110, open_id = 120, settings_id, visibility_id, exit_id, menu_id,
                 list_id = 130, edit_id, pause_id, erase_id, new_id, postpone_id,
                 force_id = 180, kind_id = 140, date_id, time_id, daily_id, weekday_id = 150, theme_id = 160, locale_id = 170 };
inline constexpr int content_height = 888; // logical px of scrollable content, tallest footer state (countdown + error)
inline constexpr DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VSCROLL;

inline ClockSample clock_sample() {
    FILETIME file{};
    GetSystemTimeAsFileTime(&file);
    ULARGE_INTEGER ticks{};
    ticks.LowPart = file.dwLowDateTime; ticks.HighPart = file.dwHighDateTime;
    return {(ticks.QuadPart - 116444736000000000ULL) / 10000, GetTickCount64()};
}

inline std::wstring current_user_sid() {
    HANDLE token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return L"";
    DWORD length = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &length);
    std::vector<BYTE> data(length);
    const bool ok = length && GetTokenInformation(token, TokenUser, data.data(), length, &length);
    CloseHandle(token);
    if (!ok) return L"";
    LPWSTR sid{};
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid, &sid)) return L"";
    const std::wstring result(sid);
    LocalFree(sid);
    return result;
}

inline std::wstring legacy_data_file() {
    wchar_t local[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local))) return L"";
    const auto directory = std::wstring(local) + L"\\PowerTimer";
    return directory + L"\\schedules-v1.dat";
}

inline std::wstring data_file() {
    if (dedicated_process) {
        wchar_t directory[MAX_PATH]{};
        const auto sid = current_user_sid();
        if (!mod_storage_path || !mod_storage_path(directory, std::size(directory)) || sid.empty()) return L"";
        const auto path = std::wstring(directory) + L"\\schedules-" + sid + L".dat";
        const DWORD attributes = GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES) return path;
        if (GetLastError() != ERROR_FILE_NOT_FOUND) return L"";
        const auto legacy = legacy_data_file();
        if (legacy.empty()) return L"";
        if (!legacy.empty() && !CopyFileW(legacy.c_str(), path.c_str(), TRUE)) {
            const DWORD error = GetLastError();
            // Never replace an existing destination or hide a failed migration.
            if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND &&
                error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) return L"";
        }
        return path;
    }
    wchar_t local[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, local))) return L"";
    const auto directory = std::wstring(local) + L"\\PowerTimer";
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return L"";
    return directory + L"\\preview-v1.dat";
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

// Clock glyph rendered at the exact icon size with 4x4 supersampling, so the
// tray never shows a downscaled, aliased bitmap.
inline HICON make_icon(int size) {
    size = (std::max)(size, 16);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* bits{};
    HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!color) return nullptr;
    auto* pixels = static_cast<DWORD*>(bits);
    const double s = size, c = s / 2, stroke = (std::max)(s * 0.085, 1.25) / 2;
    const auto capsule = [](double px, double py, double ax, double ay, double bx, double by) {
        const double vx = bx - ax, vy = by - ay;
        const double t = (std::clamp)(((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy), 0.0, 1.0);
        return std::hypot(px - ax - vx * t, py - ay - vy * t);
    };
    for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x) {
        double alpha = 0, white = 0, shade = 0;
        for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx) {
            const double px = x + (sx + 0.5) / 4, py = y + (sy + 0.5) / 4;
            const double r = std::hypot(px - c, py - c);
            if (r > s * 0.47) continue;
            alpha += 1;
            shade += py / s;
            if (std::abs(r - s * 0.31) < stroke || capsule(px, py, c, c, c, c - s * 0.2) < stroke ||
                capsule(px, py, c, c, c + s * 0.15, c + s * 0.06) < stroke) white += 1;
        }
        if (alpha == 0) { pixels[y * size + x] = 0; continue; }
        // Vertical gradient from #2B8FE8 to #0063B1, white glyph on top.
        const double t = shade / alpha, w = white / alpha;
        const auto mix = [&](double top, double bottom) {
            return static_cast<DWORD>(std::lround((top + (bottom - top) * t) * (1 - w) + 255 * w));
        };
        pixels[y * size + x] = (static_cast<DWORD>(std::lround(alpha / 16 * 255)) << 24) |
            (mix(0x2B, 0x00) << 16) | (mix(0x8F, 0x63) << 8) | mix(0xE8, 0xB1);
    }
    HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
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
    HFONT fonts_[5]{};
    HBRUSH background_{}, field_brush_{};
    HICON icon_{}, big_icon_{};
    SimulatedBackend simulated_backend_;
    WindowsPowerBackend real_backend_;
    PowerBackend& backend() { return simulation_mode ? static_cast<PowerBackend&>(simulated_backend_) : real_backend_; }
    HWND force_{};
    std::vector<std::pair<HWND, std::pair<const wchar_t*, const wchar_t*>>> localized_;
    bool force_on_ = false;
    HWND window_{}, action_{}, duration_{}, preview_{}, status_{}, countdown_{}, detail_{}, empty_{}, error_{}, start_{}, cancel_{}, postpone_{};
    HWND footer_notes_[2]{};
    int footer_state_ = -1, content_ = content_height, hot_row_ = -1;
    HWND hot_button_{};
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
    WarningTracker warnings_;
    NOTIFYICONDATAW tray_{};
    UINT taskbar_created_{};
    UINT dpi_ = 96;
    int scroll_offset_ = 0;
    bool tray_added_ = false;
    std::uint64_t last_tray_try_{};
    COLORREF bg_ = RGB(247, 249, 252), ink_ = RGB(24, 35, 53), accent_ = RGB(0, 120, 212), muted_ = RGB(96, 106, 122);
    bool high_contrast_ = false, dark_ = false;
    unsigned theme_value_ = 0; // 0=automatic, 1=light, 2=dark
    unsigned locale_ = 1; // 0=pt-BR, 1=English
    bool initialize_locale_ = false;

    int px(int value) const { return MulDiv(value, static_cast<int>(dpi_), 96); }
    void scroll_to(int position) {
        if (!window_) return;
        RECT client{};
        GetClientRect(window_, &client);
        const int limit = (std::max)(0, px(content_) - static_cast<int>(client.bottom));
        scroll_offset_ = (std::clamp)(position, 0, limit);
        SCROLLINFO info{};
        info.cbSize = sizeof(info);
        info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        info.nMin = 0;
        info.nMax = px(content_) - 1;
        info.nPage = static_cast<UINT>(client.bottom);
        info.nPos = scroll_offset_;
        SetScrollInfo(window_, SB_VERT, &info, TRUE);
        for (const auto& item : items_)
            MoveWindow(item.handle, px(item.x), px(item.y) - scroll_offset_, px(item.width), px(item.height), TRUE);
        InvalidateRect(window_, nullptr, TRUE);
    }
    void reveal(HWND control) {
        if (!control || !window_) return;
        RECT bounds{}, client{};
        GetWindowRect(control, &bounds);
        MapWindowPoints(nullptr, window_, reinterpret_cast<POINT*>(&bounds), 2);
        GetClientRect(window_, &client);
        if (bounds.top < 0) scroll_to(scroll_offset_ + bounds.top);
        else if (bounds.bottom > client.bottom) scroll_to(scroll_offset_ + bounds.bottom - client.bottom);
    }
    HWND add(const wchar_t* type, const wchar_t* label, DWORD style, int id,
             int x, int y, int width, int height, int font = 0, DWORD ex = 0) {
        if (wcscmp(type, L"BUTTON") == 0) style |= BS_NOTIFY;
        HWND control = CreateWindowExW(ex, type, label, WS_CHILD | WS_VISIBLE | style,
            px(x), px(y), px(width), px(height), window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleW(nullptr), nullptr);
        if (!control) throw GetLastError();
        if (wcscmp(type, L"BUTTON") == 0 && (style & BS_TYPEMASK) == BS_OWNERDRAW) SetWindowSubclass(control, button_proc, 2, reinterpret_cast<DWORD_PTR>(this));
        items_.push_back({control, x, y, width, height, font});
        return control;
    }
    // Creates a control whose caption follows the selected language.
    HWND add_l(const wchar_t* type, const wchar_t* pt, const wchar_t* en, DWORD style, int id,
               int x, int y, int width, int height, int font = 0, DWORD ex = 0) {
        HWND control = add(type, get_text(locale_, pt, en), style, id, x, y, width, height, font, ex);
        localized_.push_back({control, {pt, en}});
        return control;
    }
    void fill_choice_combos() {
        if (action_) {
            const auto keep = SendMessageW(action_, CB_GETCURSEL, 0, 0);
            SendMessageW(action_, CB_RESETCONTENT, 0, 0);
            for (std::size_t i = 0; i < std::size(text::actions); ++i)
                SendMessageW(action_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(get_text(locale_, text::actions[i], text_en::actions[i])));
            SendMessageW(action_, CB_SETCURSEL, keep < 0 ? 0 : keep, 0);
        }
        if (kind_) {
            const auto keep = SendMessageW(kind_, CB_GETCURSEL, 0, 0);
            SendMessageW(kind_, CB_RESETCONTENT, 0, 0);
            for (std::size_t i = 0; i < std::size(text::kinds); ++i)
                SendMessageW(kind_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(get_text(locale_, text::kinds[i], text_en::kinds[i])));
            SendMessageW(kind_, CB_SETCURSEL, keep < 0 ? 0 : keep, 0);
        }
    }
    void refresh_locale_text() {
        if (!window_) return;
        SetWindowTextW(window_, get_text(locale_, text::title, text_en::title));
        for (const auto& item : localized_) SetWindowTextW(item.first, get_text(locale_, item.second.first, item.second.second));
        // Texts that depend on state.
        set_text(start_, editing_id_ ? get_text(locale_, text::save_schedule, text_en::save_schedule) : get_text(locale_, text::add_schedule, text_en::add_schedule));
        if (pause_) set_text(pause_, selected_record() && selected_record()->paused ? get_text(locale_, text::resume, text_en::resume) : get_text(locale_, text::pause, text_en::pause));
        fill_choice_combos();
        for (int i = 0; i < static_cast<int>(std::size(text::presets)); ++i)
            if (HWND preset = GetDlgItem(window_, preset_id + i)) SetWindowTextW(preset, get_text(locale_, text::presets[i].label, text_en::presets[i].label));
        for (int i = 0; i < 7; ++i)
            if (days_[i]) SetWindowTextW(days_[i], get_text(locale_, text::weekdays[i], text_en::weekdays[i]));
        if (list_) {
            const wchar_t* labels[] = {get_text(locale_, text::id_column, text_en::id_column), get_text(locale_, text::action_column, text_en::action_column), get_text(locale_, text::due_column, text_en::due_column), get_text(locale_, text::state_column, text_en::state_column), get_text(locale_, text::rule_column, text_en::rule_column)};
            const int widths[] = {40, 80, 172, 130, 80};
            for (int i = 0; i < 5; ++i) {
                LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
                column.pszText = const_cast<wchar_t*>(labels[i]); column.cx = px(widths[i]);
                ListView_SetColumn(list_, i, &column);
            }
        }
        list_signature_.clear();

        // Refresh dynamic content
        preview();
        refresh();
    }

    void fonts_and_layout() {
        for (auto& font : fonts_) { if (font) DeleteObject(font); font = nullptr; }
        const int sizes[] = {14, 25, 11, 40, 12};
        for (int i = 0; i < 5; ++i)
            fonts_[i] = CreateFontW(-px(sizes[i]), 0, 0, 0, i == 1 || i == 3 || i == 4 ? FW_SEMIBOLD : FW_NORMAL,
                FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        for (const auto& item : items_) {
            MoveWindow(item.handle, px(item.x), px(item.y) - scroll_offset_, px(item.width), px(item.height), TRUE);
            SendMessageW(item.handle, WM_SETFONT, reinterpret_cast<WPARAM>(fonts_[item.font]), TRUE);
        }
        if (list_) {
            const int widths[] = {40, 80, 172, 130, 80};
            for (int i = 0; i < 5; ++i) ListView_SetColumnWidth(list_, i, px(widths[i]));
        }
        if (list_) { // a 12 x 32 image list gives the rows their height and left padding
            if (const auto images = ImageList_Create(px(12), px(32), ILC_COLOR, 1, 1)) {
                if (const auto old = ListView_SetImageList(list_, images, LVSIL_SMALL)) ImageList_Destroy(old);
            }
        }
        round_controls();
        for (HWND combo : {action_, kind_}) if (combo) {
            SendMessageW(combo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), px(24));
            SendMessageW(combo, CB_SETITEMHEIGHT, 0, px(36));
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

        dark_ = false;
        if (high_contrast_) {
            bg_ = GetSysColor(COLOR_WINDOW);
            ink_ = GetSysColor(COLOR_WINDOWTEXT);
            accent_ = GetSysColor(COLOR_HIGHLIGHT);
            muted_ = GetSysColor(COLOR_GRAYTEXT);
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

            dark_ = use_dark;
            if (use_dark) {
                // Dark theme colors
                bg_ = RGB(30, 30, 30);
                ink_ = RGB(240, 240, 240);
                accent_ = RGB(0, 120, 212); // Keep our brand blue
                muted_ = RGB(165, 165, 165);
            } else {
                // Light theme colors
                bg_ = RGB(247, 249, 252);
                ink_ = RGB(24, 35, 53);
                accent_ = RGB(0, 120, 212); // Our brand blue
                muted_ = RGB(96, 106, 122);
            }
        }

        if (background_) DeleteObject(background_);
        background_ = CreateSolidBrush(bg_);
        if (window_) RedrawWindow(window_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN | RDW_UPDATENOW);
        if (list_) {
            ListView_SetTextColor(list_, ink_);
            ListView_SetTextBkColor(list_, high_contrast_ ? bg_ : CLR_NONE);
        }
        apply_control_themes();
    }
    // Dark variants of the stock edit/combo/list/scrollbar visuals (Windows 10 1809+ / 11).
    // Looked up at run time so the mod needs no extra import library; harmless if missing.
    // Rounded outline for the edit and the list: clip them to a round rect, the frame is painted in paint_frame.
    void round_controls() {
        for (HWND control : {list_, date_, time_}) if (control) {
            RECT bounds{}; GetWindowRect(control, &bounds);
            SetWindowRgn(control, high_contrast_ ? nullptr : CreateRoundRectRgn(0, 0, bounds.right - bounds.left + 1, bounds.bottom - bounds.top + 1, px(16), px(16)), TRUE);
        }
    }
    // Caption blends into the body: dark/light caption plus body-coloured caption and border (Windows 11).
    void apply_caption() {
        if (!window_) return;
        using DwmSetFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
        static const auto set_attribute = reinterpret_cast<DwmSetFn>([] {
            const auto module = LoadLibraryExW(L"dwmapi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            return module ? GetProcAddress(module, "DwmSetWindowAttribute") : nullptr;
        }());
        if (!set_attribute) return;
        constexpr DWORD dark_attribute = 20, border_attribute = 34, caption_attribute = 35, corner_attribute = 33;
        const DWORD corner = high_contrast_ ? 0 : 2; // DWMWCP_ROUND
        set_attribute(window_, corner_attribute, &corner, sizeof(corner));
        const BOOL dark = dark_ && !high_contrast_;
        const COLORREF color = high_contrast_ ? 0xFFFFFFFF : bg_;
        set_attribute(window_, dark_attribute, &dark, sizeof(dark));
        set_attribute(window_, caption_attribute, &color, sizeof(color));
        set_attribute(window_, border_attribute, &color, sizeof(color));
    }
    void apply_control_themes() {
        using SetWindowThemeFn = HRESULT(WINAPI*)(HWND, LPCWSTR, LPCWSTR);
        static const auto set_theme = reinterpret_cast<SetWindowThemeFn>([] {
            const auto module = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            return module ? GetProcAddress(module, "SetWindowTheme") : nullptr;
        }());
        apply_caption();
        if (!set_theme) return;
        const bool dark = dark_ && !high_contrast_;
        const auto apply = [&](HWND control, const wchar_t* dark_name) {
            if (control) set_theme(control, dark ? dark_name : nullptr, nullptr);
        };
        apply(duration_, L"DarkMode_CFD");
        for (HWND combo : {action_, kind_}) apply(combo, L"DarkMode_CFD");
        for (HWND day : days_) apply(day, L"DarkMode_Explorer");
        apply(window_, L"DarkMode_Explorer");
        round_controls();
        apply(date_, L"DarkMode_CFD");
        apply(time_, L"DarkMode_CFD");
        if (list_) {
            ListView_SetTextColor(list_, ink_);
            ListView_SetTextBkColor(list_, high_contrast_ ? bg_ : CLR_NONE);
            ListView_SetBkColor(list_, high_contrast_ ? bg_ : field_color());
            apply(list_, L"DarkMode_ItemsView"); // Explorer theme draws column separators
            apply(ListView_GetHeader(list_), L"DarkMode_ItemsView");
        }
    }
    void controls() {
        add_l(L"STATIC", text::brand, text_en::brand, 0, 0, 24, 20, 360, 36, 1);
        if (simulation_mode) add_l(L"STATIC", text::simulation, text_en::simulation, SS_RIGHT, 0, 454, 28, 102, 20, 2);
        add_l(L"STATIC", text::subtitle, text_en::subtitle, 0, 0, 24, 60, 360, 22);
        add_l(L"BUTTON", text::options, text_en::options, WS_TABSTOP | BS_OWNERDRAW, menu_id, 484, 56, 72, 32, 4);

        constexpr DWORD combo_style = WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL;
        add_l(L"STATIC", text::action_label, text_en::action_label, 0, 0, 24, 104, 254, 22);
        action_ = add(L"COMBOBOX", L"", combo_style, action_id, 24, 128, 254, 180);
        add_l(L"STATIC", text::kind_label, text_en::kind_label, 0, 0, 304, 104, 252, 22);
        kind_ = add(L"COMBOBOX", L"", combo_style, kind_id, 304, 128, 252, 160);
        for (HWND combo : {action_, kind_}) SetWindowSubclass(combo, combo_proc, 1, reinterpret_cast<DWORD_PTR>(this));
        SendMessageW(action_, CB_SETCURSEL, 0, 0);
        SendMessageW(kind_, CB_SETCURSEL, 0, 0);
        duration_controls_.push_back(add_l(L"STATIC", text::duration_label, text_en::duration_label, 0, 0, 24, 176, 254, 22));
        duration_ = add(L"EDIT", L"30", WS_TABSTOP | ES_AUTOHSCROLL, duration_id,
                        36, 206, 230, 20); // the rounded field around it is painted by paint_duration_field
        SendMessageW(duration_, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
        duration_controls_.push_back(duration_);
        SendMessageW(duration_, EM_SETLIMITTEXT, 32, 0);
        duration_controls_.push_back(add_l(L"STATIC", text::duration_hint, text_en::duration_hint, 0, 0, 304, 206, 252, 22, 2));
        for (int i = 0; i < static_cast<int>(std::size(text::presets)); ++i)
            duration_controls_.push_back(add(L"BUTTON", get_text(locale_, text::presets[i].label, text_en::presets[i].label), WS_TABSTOP | BS_OWNERDRAW, preset_id + i,
                24 + 135 * i, 248, 127, 32, 4));
        once_controls_.push_back(add_l(L"STATIC", text::date_label, text_en::date_label, 0, 0, 24, 176, 254, 22));
        date_ = add(DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_SHORTDATEFORMAT, date_id, 24, 200, 254, 32);
        SYSTEMTIME range[2]{}; range[0].wYear = 1970; range[0].wMonth = 1; range[0].wDay = 1;
        range[1].wYear = 9999; range[1].wMonth = 12; range[1].wDay = 31;
        DateTime_SetRange(date_, GDTR_MIN | GDTR_MAX, range);
        once_controls_.push_back(date_);
        calendar_controls_.push_back(add_l(L"STATIC", text::time_label, text_en::time_label, 0, 0, 304, 176, 252, 22));
        time_ = add(DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_TIMEFORMAT | DTS_UPDOWN, time_id, 304, 200, 252, 32);
        SendMessageW(time_, DTM_SETFORMATW, 0, reinterpret_cast<LPARAM>(L"HH:mm"));
        for (HWND picker : {date_, time_}) SetWindowSubclass(picker, picker_proc, 1, reinterpret_cast<DWORD_PTR>(this));
        calendar_controls_.push_back(time_);
        weekly_controls_.push_back(add_l(L"STATIC", text::weekly_label, text_en::weekly_label, 0, 0, 24, 176, 254, 22, 2));
        weekly_controls_.push_back(add_l(L"BUTTON", text::daily, text_en::daily, WS_TABSTOP | BS_OWNERDRAW, daily_id, 24, 200, 254, 32, 4));
        for (int i = 0; i < 7; ++i) {
            days_[i] = add(L"BUTTON", get_text(locale_, text::weekdays[i], text_en::weekdays[i]), WS_TABSTOP | BS_AUTOCHECKBOX, weekday_id + i,
                           24 + 77 * i, 248, 70, 32, 2);
            SendMessageW(days_[i], BM_SETCHECK, BST_CHECKED, 0);
            weekly_controls_.push_back(days_[i]);
        }
        preview_ = add(L"STATIC", L"", 0, 0, 24, 296, 300, 32, 2);
        force_ = add_l(L"BUTTON", text::force_label, text_en::force_label, WS_TABSTOP | BS_OWNERDRAW, force_id, 330, 296, 226, 32, 4);
        start_ = add_l(L"BUTTON", text::add_schedule, text_en::add_schedule, WS_TABSTOP | BS_OWNERDRAW, schedule_id,
                     24, 344, 348, 40, 4);
        add_l(L"BUTTON", text::new_schedule, text_en::new_schedule, WS_TABSTOP | BS_OWNERDRAW, new_id, 380, 344, 176, 40, 4);
        add_l(L"STATIC", text::schedules, text_en::schedules, 0, 0, 24, 400, 532, 22);
        list_ = add(WC_LISTVIEWW, L"", WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER,
                    list_id, 24, 424, 532, 160, 2);
        ListView_SetExtendedListViewStyle(list_, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SetWindowSubclass(list_, list_proc, 1, reinterpret_cast<DWORD_PTR>(this));
        const wchar_t* labels[] = {get_text(locale_, text::id_column, text_en::id_column), get_text(locale_, text::action_column, text_en::action_column), get_text(locale_, text::due_column, text_en::due_column), get_text(locale_, text::state_column, text_en::state_column), get_text(locale_, text::rule_column, text_en::rule_column)};
        const int widths[] = {40, 80, 172, 130, 80};
        for (int i = 0; i < 5; ++i) {
            LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(labels[i]); column.cx = px(widths[i]);
            ListView_InsertColumn(list_, i, &column);
        }
        edit_ = add_l(L"BUTTON", text::edit, text_en::edit, WS_TABSTOP | BS_OWNERDRAW, edit_id, 24, 596, 76, 32, 4);
        pause_ = add_l(L"BUTTON", text::pause, text_en::pause, WS_TABSTOP | BS_OWNERDRAW, pause_id, 108, 596, 84, 32, 4);
        erase_ = add_l(L"BUTTON", text::erase, text_en::erase, WS_TABSTOP | BS_OWNERDRAW, erase_id, 200, 596, 76, 32, 4);
        cancel_ = add_l(L"BUTTON", text::cancel, text_en::cancel, WS_TABSTOP | BS_OWNERDRAW, cancel_id, 284, 596, 140, 32, 4);
        postpone_ = add_l(L"BUTTON", text::postpone, text_en::postpone, WS_TABSTOP | BS_OWNERDRAW, postpone_id, 432, 596, 124, 32, 4);
        status_ = add_l(L"STATIC", text::idle, text_en::idle, SS_CENTER, 0, 24, 648, 532, 24);
        countdown_ = add_l(L"STATIC", text::clock_empty, text_en::clock_empty, SS_CENTER, 0, 24, 672, 532, 56, 3);
        detail_ = add_l(L"STATIC", text::idle_detail, text_en::idle_detail, SS_CENTER, 0, 24, 732, 532, 40, 2);
        empty_ = add_l(L"STATIC", text::no_active_schedule, text_en::no_active_schedule, SS_CENTER, 0, 24, 648, 532, 20, 2);
        error_ = add(L"STATIC", L"", SS_CENTER, 0, 24, 780, 532, 32, 2);
        footer_notes_[0] = add_l(L"STATIC", text::simulation_note, text_en::simulation_note, SS_CENTER, 0, 24, 820, 532, 22, 2);
        if (simulation_mode) footer_notes_[1] = add_l(L"STATIC", text::settings_note, text_en::settings_note, SS_CENTER, 0, 24, 844, 532, 22, 2);
        fill_choice_combos();
        apply_control_themes();
        fonts_and_layout();
        show_kind();
    }
    // Window height follows the footer, keeping the bottom edge in place.
    void fit_window() {
        if (!window_ || IsIconic(window_)) return;
        RECT bounds{}, size{0, 0, px(580), px(content_)};
        GetWindowRect(window_, &bounds);
        MONITORINFO info{}; info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &info)) return;
        AdjustWindowRectExForDpi(&size, window_style, FALSE, WS_EX_CONTROLPARENT, dpi_);
        const int height = (std::min)(static_cast<int>(size.bottom - size.top), static_cast<int>(info.rcWork.bottom - info.rcWork.top));
        SetWindowPos(window_, nullptr, bounds.left, (std::max)(static_cast<int>(info.rcWork.top), static_cast<int>(bounds.bottom) - height),
            bounds.right - bounds.left, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    // Footer height follows the state: 0 = countdown, 1 = status + detail only, 2 = nothing active.
    void layout_footer(int state, bool has_error, int lines) {
        const int key = (state * 2 + has_error) * 4 + lines;
        if (!status_ || key == footer_state_) return;
        footer_state_ = key;
        ShowWindow(status_, state == 2 ? SW_HIDE : SW_SHOW);
        ShowWindow(countdown_, state == 0 ? SW_SHOW : SW_HIDE);
        ShowWindow(detail_, state == 2 ? SW_HIDE : SW_SHOW);
        ShowWindow(empty_, state == 2 ? SW_SHOW : SW_HIDE);
        const int detail_top = state == 0 ? 732 : 680, detail_height = 16 * lines;
        const int end = state == 0 || state == 1 ? detail_top + detail_height : 668;
        const auto place = [&](HWND handle, int y, int height = 0) {
            for (auto& item : items_) {
                if (item.handle == handle) {
                    item.y = y;
                    if (height) item.height = height;
                }
            }
        };
        if (state != 2) place(detail_, detail_top, detail_height);
        else { place(status_, 648); place(detail_, 648, detail_height); }
        if (state != 0) place(countdown_, state == 1 ? 680 : 648); // hidden, kept inside the content area
        if (state == 0) place(countdown_, 672);
        const int block = end + (has_error ? 40 : 0);
        place(error_, end + 8);
        place(footer_notes_[0], block + 16);
        place(footer_notes_[1], block + 46);
        content_ = block + (footer_notes_[1] ? 46 : 16) + 22 + 16;
        fit_window();
        scroll_to(scroll_offset_);
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
        const bool sound = notification_sound_setting ? notification_sound_setting() : book_.get_sound_enabled();
        const UINT sound_flag = sound ? 0 : NIIF_NOSOUND;
        notice.dwInfoFlags = NIIF_INFO | sound_flag | NIIF_RESPECT_QUIET_TIME;
        Shell_NotifyIconW(NIM_MODIFY, &notice);
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
            std::wstring message = std::wstring(get_text(locale_, text::conflict_error, text_en::conflict_error)) + get_text(locale_, text::schedule_number, text_en::schedule_number) + std::to_wstring(book_.conflicting_id());
            for (const auto& record : book_.list()) if (record.id == book_.conflicting_id()) {
                message += std::wstring(get_text(locale_, text::separator, text_en::separator)) + get_text(locale_, text::actions[static_cast<int>(record.action)], text_en::actions[static_cast<int>(record.action)]) +
                    std::wstring(get_text(locale_, text::separator, text_en::separator)) + local_time_at(effective_due_utc_ms(record), locale_);
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
            auto due = record.state == OccurrenceState::Scheduled ? local_time_at(effective_due_utc_ms(record), locale_) : std::wstring(get_text(locale_, text::no_next, text_en::no_next));
            auto rule = record.kind == ScheduleKind::Duration ? std::to_wstring(record.minutes) + L" min" :
                std::wstring(record.kind == ScheduleKind::Once ? get_text(locale_, text::once_rule, text_en::once_rule) : get_text(locale_, text::weekly_rule, text_en::weekly_rule));
            auto action_text = std::wstring(get_text(locale_, text::actions[static_cast<int>(record.action)], text_en::actions[static_cast<int>(record.action)]));
            if (record.force_shutdown) action_text += get_text(locale_, text::forced_tag, text_en::forced_tag);
            ListView_SetItemText(list_, row, 1, action_text.data());
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
            const auto exec = execute_power_action(backend(), delivery.action, delivery.force);
            if (exec.status != ExecStatus::Accepted) {
                // Intent stays Uncertain: never retried, next weekly occurrence untouched.
                notify(get_text(locale_, text::state_uncertain, text_en::state_uncertain),
                    exec.status == ExecStatus::NotAvailable
                        ? get_text(locale_, text::power_unavailable, text_en::power_unavailable)
                        : get_text(locale_, text::power_failed, text_en::power_failed));
                continue;
            }
            const bool recorded = book_.complete(delivery);
            if (!recorded) show_error();
            notify(recorded ? get_text(locale_, text::completed, text_en::completed) : get_text(locale_, text::state_uncertain, text_en::state_uncertain),
                recorded ? get_text(locale_, text::completed_detail, text_en::completed_detail) : get_text(locale_, text::uncertain_detail, text_en::uncertain_detail));
            if (!recorded) break;
        }
        // Countdown warnings: 30, 15, 10, 5 and 1 minute (see WarningTracker).
        for (const auto& record : book_.list()) {
            if (!book_.writable()) break;
            if (record.state != OccurrenceState::Scheduled || record.paused || record.conflict_with) continue;
            const auto index = warnings_.due(record.occurrence_id, book_.remaining_ms(record.id, now));
            if (!index) continue;
            const wchar_t* title = get_text(locale_, text::warning, text_en::warning);
            const wchar_t* body = get_text(locale_, text::countdown_notification, text_en::countdown_notification);
            switch (*index) {
                case 0: title = get_text(locale_, text::warning_30min, text_en::warning_30min);
                        body = get_text(locale_, text::warning_30min_detail, text_en::warning_30min_detail); break;
                case 1: title = get_text(locale_, text::warning_15min, text_en::warning_15min);
                        body = get_text(locale_, text::warning_15min_detail, text_en::warning_15min_detail); break;
                case 2: title = get_text(locale_, text::warning_10min, text_en::warning_10min);
                        body = get_text(locale_, text::warning_10min_detail, text_en::warning_10min_detail); break;
                case 3: title = get_text(locale_, text::warning_5min, text_en::warning_5min);
                        body = get_text(locale_, text::warning_5min_detail, text_en::warning_5min_detail); break;
                default: break; // 1 minute: the default texts above
            }
            notify(title, body);
            // Only the 1-minute warning brings the panel up, to avoid too many popups.
            if (*index == 4) show(false);
        }
        std::wstring conflicts;
        for (const auto& record : book_.list()) if (record.conflict_with)
            conflicts += std::to_wstring(record.id) + L":" + std::to_wstring(record.conflict_with) + L";";
        if (conflicts != conflict_signature_) {
            conflict_signature_ = conflicts;
            if (!conflicts.empty()) notify(get_text(locale_, text::state_conflict, text_en::state_conflict), get_text(locale_, text::conflict_notification, text_en::conflict_notification));
        }
        sync_list();
        int want = 2, lines = 1;
        const auto* record = selected_record();
        const bool writable = book_.writable();
        const bool scheduled = record && record->state == OccurrenceState::Scheduled;
        EnableWindow(start_, writable);
        EnableWindow(edit_, writable && record);
        EnableWindow(erase_, writable && record);
        EnableWindow(pause_, writable && scheduled);
        EnableWindow(cancel_, writable && scheduled);
        EnableWindow(postpone_, writable && scheduled);
        set_text(pause_, record && record->paused ? get_text(locale_, text::resume, text_en::resume) : get_text(locale_, text::pause, text_en::pause));
        set_text(start_, editing_id_ ? get_text(locale_, text::save_schedule, text_en::save_schedule) : get_text(locale_, text::add_schedule, text_en::add_schedule));
        if (record) {
            set_text(status_, state_label(*record));
            if (scheduled && !record->paused && !record->conflict_with) {
                const auto seconds = (book_.remaining_ms(record->id, now) + 999) / 1000;
                wchar_t value[48]{};
                swprintf_s(value, L"%02llu:%02llu:%02llu", seconds / 3600, (seconds / 60) % 60, seconds % 60);
                set_text(countdown_, value);
                want = 0;
            } else { set_text(countdown_, get_text(locale_, text::clock_empty, text_en::clock_empty)); want = 1; }
            const wchar_t* note = record->state == OccurrenceState::Uncertain ? get_text(locale_, text::uncertain_detail, text_en::uncertain_detail) :
                record->paused && scheduled ? get_text(locale_, text::paused_detail, text_en::paused_detail) : nullptr;
            std::wstring detail = note ? std::wstring(note) : std::wstring(get_text(locale_, text::actions[static_cast<int>(record->action)], text_en::actions[static_cast<int>(record->action)])) +
                get_text(locale_, text::separator, text_en::separator) + local_time_at(effective_due_utc_ms(*record), locale_);
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
            lines = 1 + static_cast<int>(std::count(detail.begin(), detail.end(), L'\n'));
        } else {
            set_text(status_, get_text(locale_, text::idle, text_en::idle));
            set_text(countdown_, get_text(locale_, text::clock_empty, text_en::clock_empty));
            set_text(detail_, get_text(locale_, text::idle_detail, text_en::idle_detail));
        }
        if (!writable) show_error();
        layout_footer(want, GetWindowTextLengthW(error_) > 0, (std::min)(lines, 2));
        preview();
        if (!tray_added_ && now.monotonic_ms - last_tray_try_ >= 5000) add_tray();
    }
    // Forced close only applies to Shutdown; any other action clears it.
    void set_force_on(bool on) { force_on_ = on; if (force_) InvalidateRect(force_, nullptr, TRUE); }
    void sync_force() {
        if (!force_) return;
        const bool shutdown = selected_action() == Action::Shutdown;
        if (!shutdown) set_force_on(false);
        EnableWindow(force_, shutdown);
    }
    void confirm_force() {
        set_force_on(!force_on_);  // owner-drawn: toggle by hand
        if (!force_on_) return;
        if (MessageBoxW(window_, get_text(locale_, text::force_warning, text_en::force_warning),
                get_text(locale_, text::force_title, text_en::force_title), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
            set_force_on(false);
    }
    bool force_checked() const {
        return force_ && force_on_ && selected_action() == Action::Shutdown;
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
            if (backend().query().of(action) != Availability::Available) {
                set_text(error_, get_text(locale_, text::power_unavailable, text_en::power_unavailable));
                return;
            }
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
                ok = book_.set_force_shutdown(selected_id_, force_checked());
                editing_id_ = 0;
            }
            show_error(); refresh();
        } else if (id == new_id) {
            editing_id_ = 0; set_text(error_, L"");
            set_force_on(false); sync_force(); refresh();
            SetFocus(selected_kind() == ScheduleKind::Duration ? duration_ : time_);
        } else if (id == edit_id) {
            if (const auto* record = selected_record()) {
                editing_id_ = record->id;
                SendMessageW(action_, CB_SETCURSEL, static_cast<WPARAM>(record->action), 0);
                set_force_on(record->force_shutdown); sync_force();
                SendMessageW(kind_, CB_SETCURSEL, static_cast<WPARAM>(record->kind), 0);
                set_text(duration_, std::to_wstring(record->minutes));
                if (record->kind != ScheduleKind::Duration) {
                    const auto civil = record->kind == ScheduleKind::Once ? calendar_.local(record->due_utc_ms).value_or(record->civil) : record->civil;
                    SYSTEMTIME selected{};
                    selected.wYear = static_cast<WORD>(civil.year); selected.wMonth = static_cast<WORD>(civil.month);
                    selected.wDay = static_cast<WORD>(civil.day);
                    selected.wHour = static_cast<WORD>(civil.hour);
                    selected.wMinute = static_cast<WORD>(civil.minute);
                    DateTime_SetSystemtime(date_, GDT_VALID, &selected); DateTime_SetSystemtime(time_, GDT_VALID, &selected);
                    for (unsigned i = 0; i < 7; ++i) SendMessageW(days_[i], BM_SETCHECK, record->weekdays & (1u << i) ? BST_CHECKED : BST_UNCHECKED, 0);
                }
                const auto focus = record->kind == ScheduleKind::Duration ? duration_ : time_;
                show_kind(); refresh(); SetFocus(focus);
            }
        } else if (id == pause_id || id == erase_id || id == cancel_id || id == postpone_id) {
            if (const auto* record = selected_record()) {
                const auto selected = record->id;
                const auto occurrence = record->occurrence_id;
                if (id == postpone_id) {
                    const auto now = clock_sample();
                    if (book_.postpone(selected, 10, now)) {
                        // Only warnings still ahead of the new deadline may fire again.
                        warnings_.reopen_ahead_of(occurrence, book_.remaining_ms(selected, now));
                    }
                } else if (id == pause_id) book_.set_paused(selected, !record->paused, clock_sample());
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
        } else if (id >= theme_id && id < theme_id + static_cast<int>(std::size(text::theme_strings))) {
            theme_value_ = static_cast<unsigned>(id - theme_id);
            if (!book_.set_theme(theme_value_)) show_error();
            colors();
            refresh_locale_text();
        } else if (id == locale_id || id == locale_id + 1) {
            const auto locale = static_cast<unsigned>(id - locale_id);
            if (!book_.set_locale(locale)) show_error();
            locale_ = locale;
            refresh_locale_text();
        } else if (id == open_id || id == settings_id) {
            show(true);
            if (id == settings_id) {
                // Settings (theme, language) live in the Options menu.
                set_text(error_, L"");
                PostMessageW(window_, WM_COMMAND, menu_id, 0);
            } else {
                SetWindowPos(window_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                FlashWindow(window_, FALSE);
            }
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
        // Open/Settings only make sense while the panel is hidden or minimised.
        if (!IsWindowVisible(window_) || IsIconic(window_)) {
            AppendMenuW(menu, MF_STRING, open_id, get_text(locale_, text::open, text_en::open));
            AppendMenuW(menu, MF_STRING, settings_id, get_text(locale_, text::settings, text_en::settings));
        }
        const auto radio = [&](const wchar_t* title, int first, const wchar_t* const* pt, const wchar_t* const* en, std::size_t count, unsigned current) {
            HMENU sub = CreatePopupMenu();
            if (!sub) return;
            for (std::size_t i = 0; i < count; ++i) {
                MENUITEMINFOW item{}; item.cbSize = sizeof(item);
                item.fMask = MIIM_STRING | MIIM_ID | MIIM_FTYPE | MIIM_STATE;
                item.fType = MFT_STRING | MFT_RADIOCHECK;
                item.fState = i == current ? MFS_CHECKED : MFS_UNCHECKED;
                item.wID = static_cast<UINT>(first + static_cast<int>(i));
                item.dwTypeData = const_cast<wchar_t*>(get_text(locale_, pt[i], en[i]));
                InsertMenuItemW(sub, static_cast<UINT>(i), TRUE, &item);
            }
            AppendMenuW(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(sub), title);
        };
        const wchar_t* const languages_pt[] = {text::portuguese, text::english};
        const wchar_t* const languages_en[] = {text_en::portuguese, text_en::english};
        radio(get_text(locale_, text::theme_label, text_en::theme_label), theme_id, text::theme_strings, text_en::theme_strings, std::size(text::theme_strings), theme_value_);
        radio(get_text(locale_, text::language_label, text_en::language_label), locale_id, languages_pt, languages_en, 2, locale_);
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, visibility_id, get_text(locale_, text::notification_settings, text_en::notification_settings));
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (book_.writable() && selected_record() && selected_record()->state == OccurrenceState::Scheduled ? MF_ENABLED : MF_GRAYED), postpone_id, get_text(locale_, text::postpone, text_en::postpone));
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
    // Warning-coloured checkbox, drawn by hand so it follows light/dark/high contrast.
    void draw_force(const DRAWITEMSTRUCT& draw, bool use_dark, bool disabled) {
        const COLORREF fill = high_contrast_ ? GetSysColor(COLOR_BTNFACE) : use_dark ? RGB(96, 62, 0) : RGB(255, 233, 170);
        const COLORREF edge = high_contrast_ ? GetSysColor(COLOR_BTNTEXT) : use_dark ? RGB(255, 190, 70) : RGB(200, 120, 0);
        const COLORREF text = disabled ? GetSysColor(COLOR_GRAYTEXT) : high_contrast_ ? GetSysColor(COLOR_BTNTEXT) :
            use_dark ? RGB(255, 236, 190) : RGB(60, 38, 0);
        FillRect(draw.hDC, &draw.rcItem, background_);
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, px(2), edge);
        const auto old_brush = SelectObject(draw.hDC, brush);
        const auto old_pen = SelectObject(draw.hDC, pen);
        RoundRect(draw.hDC, draw.rcItem.left, draw.rcItem.top, draw.rcItem.right, draw.rcItem.bottom, px(16), px(16));
        const int box = px(18), top = (draw.rcItem.top + draw.rcItem.bottom - box) / 2, left = draw.rcItem.left + px(10);
        SelectObject(draw.hDC, GetStockObject(NULL_BRUSH));
        Rectangle(draw.hDC, left, top, left + box, top + box);
        if (force_on_) {
            MoveToEx(draw.hDC, left + px(4), top + box / 2, nullptr);
            LineTo(draw.hDC, left + box / 2 - px(1), top + box - px(5));
            LineTo(draw.hDC, left + box - px(4), top + px(4));
        }
        SelectObject(draw.hDC, old_brush); SelectObject(draw.hDC, old_pen);
        DeleteObject(brush); DeleteObject(pen);
        HFONT old_font = fonts_[4] ? static_cast<HFONT>(SelectObject(draw.hDC, fonts_[4])) : nullptr;
        wchar_t label[128]{};
        GetWindowTextW(draw.hwndItem, label, 128);
        SetBkMode(draw.hDC, TRANSPARENT);
        SetTextColor(draw.hDC, text);
        RECT rect = draw.rcItem; rect.left = left + box + px(8); rect.right -= px(6);
        DrawTextW(draw.hDC, label, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (draw.itemState & ODS_FOCUS) { RECT focus = draw.rcItem; InflateRect(&focus, -px(3), -px(3)); DrawFocusRect(draw.hDC, &focus); }
        if (old_font) SelectObject(draw.hDC, old_font);
    }
    // Combo selection field and list rows, owner-drawn so they follow light/dark/high contrast.
    void draw_combo_item(const DRAWITEMSTRUCT& draw) {
        if (draw.itemID == static_cast<UINT>(-1)) return;
        const bool use_dark = dark_ && !high_contrast_;
        const bool field = (draw.itemState & ODS_COMBOBOXEDIT) != 0;
        const bool selected = (draw.itemState & ODS_SELECTED) != 0 && !field;
        const COLORREF fill = high_contrast_ ? GetSysColor(selected ? COLOR_HIGHLIGHT : COLOR_WINDOW) : field_color();
        const COLORREF text = high_contrast_ ? GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT) : ink_;
        HBRUSH brush = CreateSolidBrush(fill);
        FillRect(draw.hDC, &draw.rcItem, brush);
        DeleteObject(brush);
        if (selected && !high_contrast_) { // rounded accent-tinted highlight
            const COLORREF tint = use_dark ? RGB(29, 71, 103) : RGB(205, 229, 247);
            RECT bounds = draw.rcItem; InflateRect(&bounds, -px(4), -px(2));
            HBRUSH tint_brush = CreateSolidBrush(tint);
            HPEN tint_pen = CreatePen(PS_SOLID, 1, tint);
            const auto old_brush = SelectObject(draw.hDC, tint_brush);
            const auto old_pen = SelectObject(draw.hDC, tint_pen);
            RoundRect(draw.hDC, bounds.left, bounds.top, bounds.right, bounds.bottom, px(16), px(16));
            SelectObject(draw.hDC, old_brush); SelectObject(draw.hDC, old_pen);
            DeleteObject(tint_brush); DeleteObject(tint_pen);
        }
        wchar_t label[128]{};
        SendMessageW(draw.hwndItem, CB_GETLBTEXT, draw.itemID, reinterpret_cast<LPARAM>(label));
        HFONT old_font = fonts_[0] ? static_cast<HFONT>(SelectObject(draw.hDC, fonts_[0])) : nullptr;
        SetBkMode(draw.hDC, TRANSPARENT);
        SetTextColor(draw.hDC, text);
        RECT rect = draw.rcItem; rect.left += px(12); rect.right -= px(12);
        DrawTextW(draw.hDC, label, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (old_font) SelectObject(draw.hDC, old_font);
    }
    COLORREF field_color() const { return dark_ ? RGB(45, 45, 45) : RGB(255, 255, 255); }
    COLORREF border_color() const { return dark_ ? RGB(78, 78, 78) : RGB(196, 205, 220); }
    // Rounded field behind the duration edit (the edit itself has no border).
    void paint_duration_field(HDC dc) {
        if (!duration_ || high_contrast_ || !IsWindowVisible(duration_)) return;
        const RECT field{px(24), px(200) - scroll_offset_, px(278), px(232) - scroll_offset_};
        HBRUSH brush = CreateSolidBrush(field_color());
        HPEN pen = CreatePen(PS_SOLID, 1, border_color());
        const auto old_brush = SelectObject(dc, brush);
        const auto old_pen = SelectObject(dc, pen);
        RoundRect(dc, field.left, field.top, field.right, field.bottom, px(16), px(16));
        SelectObject(dc, old_brush); SelectObject(dc, old_pen);
        DeleteObject(brush); DeleteObject(pen);
        if (GetFocus() == duration_) {
            HPEN line = CreatePen(PS_SOLID, px(2), accent_);
            const auto before = SelectObject(dc, line);
            MoveToEx(dc, field.left + px(6), field.bottom - 1 - px(1), nullptr); LineTo(dc, field.right - px(6), field.bottom - 1 - px(1));
            SelectObject(dc, before); DeleteObject(line);
        }
    }
    void paint_frame(HWND window) {
        HDC dc = GetWindowDC(window);
        if (!dc) return;
        RECT rect{}; GetWindowRect(window, &rect); OffsetRect(&rect, -rect.left, -rect.top);
        const int thick = GetSystemMetricsForDpi(SM_CXBORDER, dpi_);
        HBRUSH brush = CreateSolidBrush(border_color());
        RECT edge = rect;
        for (int i = 0; i < thick; ++i) { FrameRect(dc, &edge, brush); InflateRect(&edge, -1, -1); }
        DeleteObject(brush);
        HPEN pen = CreatePen(PS_SOLID, thick, border_color());
        const auto old_pen = SelectObject(dc, pen);
        const auto old_brush = SelectObject(dc, GetStockObject(NULL_BRUSH));
        RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, px(16), px(16));
        SelectObject(dc, old_pen); SelectObject(dc, old_brush); DeleteObject(pen);
        ReleaseDC(window, dc);
    }
    // Date/time pickers paint their own box; add the same rounded outline on top.
    static LRESULT CALLBACK picker_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference) {
        auto* self = reinterpret_cast<Panel*>(reference);
        const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
        if ((message == WM_PAINT || message == WM_NCPAINT) && !self->high_contrast_) self->paint_frame(window);
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, picker_proc, 1);
        return result;
    }
    // Closed combo (light and dark): the stock control paints a boxed drop button, so draw all of it.
    void paint_combo(HWND combo) {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(combo, &paint);
        RECT client{};
        GetClientRect(combo, &client);
        const bool dropped = SendMessageW(combo, CB_GETDROPPEDSTATE, 0, 0) != 0;
        const bool focused = GetFocus() == combo || dropped;
        const bool hot = hot_button_ == combo;
        FillRect(dc, &client, background_);
        const COLORREF fill = hot && !focused ? (dark_ ? RGB(52, 52, 52) : RGB(250, 251, 253)) : field_color();
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, hot ? (dark_ ? RGB(100, 100, 100) : RGB(160, 172, 192)) : border_color());
        const auto old_brush = SelectObject(dc, brush);
        const auto old_pen = SelectObject(dc, pen);
        RoundRect(dc, client.left, client.top, client.right - 1, client.bottom - 1, px(16), px(16));
        SelectObject(dc, old_brush); SelectObject(dc, old_pen);
        DeleteObject(brush); DeleteObject(pen);
        if (focused) { // WinUI-style 2px accent line along the bottom edge
            HPEN line = CreatePen(PS_SOLID, px(2), accent_);
            const auto before_line = SelectObject(dc, line);
            const int y = client.bottom - 1 - px(1);
            MoveToEx(dc, client.left + px(6), y, nullptr); LineTo(dc, client.right - 1 - px(6), y);
            SelectObject(dc, before_line); DeleteObject(line);
        }
        wchar_t label[128]{};
        const auto row = SendMessageW(combo, CB_GETCURSEL, 0, 0);
        if (row >= 0) SendMessageW(combo, CB_GETLBTEXT, static_cast<WPARAM>(row), reinterpret_cast<LPARAM>(label));
        HFONT old_font = fonts_[0] ? static_cast<HFONT>(SelectObject(dc, fonts_[0])) : nullptr;
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, ink_);
        RECT rect = client; rect.left += px(12); rect.right -= px(36);
        DrawTextW(dc, label, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (old_font) SelectObject(dc, old_font);
        HPEN arrow = CreatePen(PS_SOLID, px(2), muted_);
        const auto before = SelectObject(dc, arrow);
        const int half = px(5), cx = client.right - px(12) - half, cy = (client.top + client.bottom) / 2;
        MoveToEx(dc, cx - half, cy - half / 2, nullptr);
        LineTo(dc, cx, cy + half / 2 + 1);
        LineTo(dc, cx + half + 1, cy - half / 2 - 1);
        SelectObject(dc, before); DeleteObject(arrow);
        EndPaint(combo, &paint);
    }
    static LRESULT CALLBACK combo_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference) {
        auto* self = reinterpret_cast<Panel*>(reference);
        if (!self->high_contrast_) {
            if (message == WM_MOUSEMOVE || message == WM_MOUSELEAVE) self->track_hot(window, message);
            if (message == WM_ERASEBKGND) return 1;
            if (message == WM_PAINT) { self->paint_combo(window); return 0; }
        }
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, combo_proc, 1);
        return DefSubclassProc(window, message, wparam, lparam);
    }
    // The header reports to its parent list view: paint it flat, without separators, and track the hot row.
    static LRESULT CALLBACK list_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference) {
        auto* self = reinterpret_cast<Panel*>(reference);
        if (message == WM_NCPAINT && !self->high_contrast_) {
            const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
            self->paint_frame(window);
            return result;
        }
        if (message == WM_MOUSEMOVE || message == WM_MOUSELEAVE) {
            int row = -1;
            if (message == WM_MOUSEMOVE) {
                LVHITTESTINFO hit{}; hit.pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
                row = ListView_HitTest(window, &hit);
                TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
                TrackMouseEvent(&track);
            }
            if (row != self->hot_row_) {
                const int old = self->hot_row_;
                self->hot_row_ = row;
                if (old >= 0) ListView_RedrawItems(window, old, old);
                if (row >= 0) ListView_RedrawItems(window, row, row);
            }
        }
        if (message == WM_NOTIFY && !self->high_contrast_ &&
            reinterpret_cast<NMHDR*>(lparam)->code == NM_CUSTOMDRAW && reinterpret_cast<NMHDR*>(lparam)->hwndFrom == ListView_GetHeader(window)) {
            const auto* draw = reinterpret_cast<NMCUSTOMDRAW*>(lparam);
            if (draw->dwDrawStage == CDDS_PREPAINT) {
                RECT bounds{}; GetClientRect(draw->hdr.hwndFrom, &bounds);
                HBRUSH brush = CreateSolidBrush(self->field_color());
                FillRect(draw->hdc, &bounds, brush); DeleteObject(brush);
                return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
            }
            if (draw->dwDrawStage == CDDS_ITEMPREPAINT) { self->draw_header_item(*draw); return CDRF_SKIPDEFAULT; }
            if (draw->dwDrawStage == CDDS_POSTPAINT) {
                // Repaint whatever the header draws beyond its last column, keeping the single divider.
                RECT bounds{}, last{}; GetClientRect(draw->hdr.hwndFrom, &bounds);
                const int count = Header_GetItemCount(draw->hdr.hwndFrom);
                if (count > 0 && Header_GetItemRect(draw->hdr.hwndFrom, count - 1, &last)) {
                    bounds.left = last.right;
                    HBRUSH brush = CreateSolidBrush(self->field_color());
                    FillRect(draw->hdc, &bounds, brush); DeleteObject(brush);
                    HPEN pen = CreatePen(PS_SOLID, 1, self->divider_color());
                    const auto old_pen = SelectObject(draw->hdc, pen);
                    MoveToEx(draw->hdc, bounds.left, bounds.bottom - 1, nullptr); LineTo(draw->hdc, bounds.right, bounds.bottom - 1);
                    SelectObject(draw->hdc, old_pen); DeleteObject(pen);
                }
            }
            return CDRF_DODEFAULT;
        }
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, list_proc, 1);
        return DefSubclassProc(window, message, wparam, lparam);
    }
    COLORREF divider_color() const { return dark_ ? RGB(70, 70, 70) : RGB(226, 231, 239); }
    void draw_header_item(const NMCUSTOMDRAW& draw) {
        HBRUSH brush = CreateSolidBrush(field_color());
        FillRect(draw.hdc, &draw.rc, brush);
        DeleteObject(brush);
        HPEN pen = CreatePen(PS_SOLID, 1, divider_color());
        const auto old_pen = SelectObject(draw.hdc, pen);
        MoveToEx(draw.hdc, draw.rc.left, draw.rc.bottom - 1, nullptr); LineTo(draw.hdc, draw.rc.right, draw.rc.bottom - 1);
        SelectObject(draw.hdc, old_pen); DeleteObject(pen);
        wchar_t label[64]{};
        HDITEMW header_item{}; header_item.mask = HDI_TEXT; header_item.pszText = label; header_item.cchTextMax = 64;
        Header_GetItem(draw.hdr.hwndFrom, static_cast<int>(draw.dwItemSpec), &header_item);
        const auto font = reinterpret_cast<HFONT>(SendMessageW(draw.hdr.hwndFrom, WM_GETFONT, 0, 0));
        HFONT old_font = font ? static_cast<HFONT>(SelectObject(draw.hdc, font)) : nullptr;
        SetBkMode(draw.hdc, TRANSPARENT);
        SetTextColor(draw.hdc, muted_);
        RECT rect = draw.rc; rect.left += draw.dwItemSpec == 0 ? px(16) : px(6); rect.right -= px(4);
        DrawTextW(draw.hdc, label, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (old_font) SelectObject(draw.hdc, old_font);
    }
    // Owner-draw buttons and combos get no hover notification, so the mouse is tracked here.
    void track_hot(HWND window, UINT message) {
        if (message == WM_MOUSEMOVE && hot_button_ != window) {
            const HWND old = hot_button_;
            hot_button_ = window;
            if (old) InvalidateRect(old, nullptr, FALSE);
            InvalidateRect(window, nullptr, FALSE);
            TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
            TrackMouseEvent(&track);
        } else if (message == WM_MOUSELEAVE && hot_button_ == window) {
            hot_button_ = nullptr;
            InvalidateRect(window, nullptr, FALSE);
        }
    }
    static LRESULT CALLBACK button_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference) {
        auto* self = reinterpret_cast<Panel*>(reference);
        self->track_hot(window, message);
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, button_proc, 2);
        return DefSubclassProc(window, message, wparam, lparam);
    }
    void draw_button(const DRAWITEMSTRUCT& draw) {
        const bool primary = draw.CtlID == schedule_id;
        const bool disabled = (draw.itemState & ODS_DISABLED) != 0;
        const bool use_dark = dark_ && !high_contrast_;
        if (draw.CtlID == force_id) { draw_force(draw, use_dark, disabled); return; }
        const bool pressed = (draw.itemState & ODS_SELECTED) != 0 && !disabled;
        const bool hot = draw.hwndItem == hot_button_ && !disabled;
        const auto shade = [](COLORREF c, int delta) {
            const auto clamp = [](int v) { return v < 0 ? 0 : v > 255 ? 255 : v; };
            return RGB(clamp(GetRValue(c) + delta), clamp(GetGValue(c) + delta), clamp(GetBValue(c) + delta));
        };
        COLORREF fill, edge, foreground;
        if (high_contrast_) {
            fill = GetSysColor(COLOR_BTNFACE); edge = GetSysColor(COLOR_BTNTEXT);
            foreground = GetSysColor(disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT);
        } else if (disabled) {
            fill = edge = use_dark ? RGB(48, 48, 48) : RGB(232, 236, 243);
            foreground = use_dark ? RGB(160, 160, 160) : RGB(96, 106, 122);
        } else if (primary) {
            fill = edge = pressed ? shade(accent_, -30) : hot ? shade(accent_, 18) : accent_;
            foreground = RGB(255, 255, 255);
        } else {
            const COLORREF base = use_dark ? RGB(58, 58, 58) : RGB(236, 240, 247);
            fill = pressed ? shade(base, use_dark ? -10 : -22) : hot ? shade(base, use_dark ? 12 : -10) : base;
            edge = use_dark ? RGB(84, 84, 84) : RGB(205, 214, 228);
            foreground = ink_;
        }
        // Owner-draw leaves the corners untouched: paint the page colour first.
        FillRect(draw.hDC, &draw.rcItem, background_);
        HBRUSH brush = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, edge);
        const auto old_brush = SelectObject(draw.hDC, brush);
        const auto old_pen = SelectObject(draw.hDC, pen);
        const bool chip = draw.CtlID >= preset_id && draw.CtlID < preset_id + std::size(text::presets);
        const int diameter = chip ? draw.rcItem.bottom - draw.rcItem.top : px(16);
        RoundRect(draw.hDC, draw.rcItem.left, draw.rcItem.top, draw.rcItem.right, draw.rcItem.bottom, diameter, diameter);
        SelectObject(draw.hDC, old_brush); SelectObject(draw.hDC, old_pen);
        DeleteObject(brush); DeleteObject(pen);
        HFONT old_font = fonts_[4] ? static_cast<HFONT>(SelectObject(draw.hDC, fonts_[4])) : nullptr;
        wchar_t label[128]{};
        GetWindowTextW(draw.hwndItem, label, 128);
        SetBkMode(draw.hDC, TRANSPARENT);
        SetTextColor(draw.hDC, foreground);
        RECT rect = draw.rcItem;
        if (pressed) OffsetRect(&rect, 0, 1);
        DrawTextW(draw.hDC, label, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if ((draw.itemState & ODS_FOCUS) && !(draw.itemState & ODS_NOFOCUSRECT)) {
            if (high_contrast_) { RECT focus = draw.rcItem; InflateRect(&focus, -px(4), -px(4)); DrawFocusRect(draw.hDC, &focus); }
            else {
                HPEN ring = CreatePen(PS_SOLID, px(2), accent_);
                const auto before_pen = SelectObject(draw.hDC, ring);
                const auto before_brush = SelectObject(draw.hDC, GetStockObject(NULL_BRUSH));
                RoundRect(draw.hDC, draw.rcItem.left + px(1), draw.rcItem.top + px(1), draw.rcItem.right - px(1), draw.rcItem.bottom - px(1), diameter, diameter);
                SelectObject(draw.hDC, before_pen); SelectObject(draw.hDC, before_brush); DeleteObject(ring);
            }
        }
        if (old_font) SelectObject(draw.hDC, old_font);
    }
public:
    Panel() : storage_(data_file()), book_(storage_, calendar_) {
        // Load theme and locale preference from storage
        auto loaded = storage_.load();
        if (loaded.status == power_timer::LoadStatus::Loaded) {
            theme_value_ = loaded.snapshot.theme;
            locale_ = loaded.snapshot.locale;
        } else if (loaded.status == LoadStatus::Missing) {
            locale_ = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_PORTUGUESE ? 0 : 1;
            initialize_locale_ = true;
        }
        // Apply theme immediately
        colors();
        refresh_locale_text();
    }
    ~Panel() {
        if (window_ && IsWindow(window_)) DestroyWindow(window_);
        for (auto font : fonts_) if (font) DeleteObject(font);
        if (background_) DeleteObject(background_);
        if (field_brush_) DeleteObject(field_brush_);
        if (icon_) DestroyIcon(icon_);
        if (big_icon_) DestroyIcon(big_icon_);
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
        } else {
            if (error_) set_text(error_, get_text(locale_, text::tray_error, text_en::tray_error));
            if (log_message) log_message(L"Tray icon registration failed; will retry without opening the panel.");
        }
    }
    bool create() {
        colors();
        icon_ = make_icon(GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem()));
        big_icon_ = make_icon(GetSystemMetricsForDpi(SM_CXICON, GetDpiForSystem()));
        taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");
        WNDCLASSW cls{};
        cls.lpfnWndProc = procedure;
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.hIcon = big_icon_;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        cls.lpszClassName = window_class;
        if (!RegisterClassW(&cls)) return false;
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
        RECT size{0, 0, px(580), px(content_height)};
        constexpr DWORD style = window_style;
        AdjustWindowRectExForDpi(&size, style, FALSE, WS_EX_CONTROLPARENT, dpi_);
        const int width = size.right - size.left;
        const int height = (std::min)(size.bottom - size.top, work.bottom - work.top);
        window_ = CreateWindowExW(WS_EX_CONTROLPARENT, window_class, get_text(locale_, text::title, text_en::title), style,
            work.right - width - px(20), (std::max)(work.top, work.bottom - height - px(20)),
            width, height, nullptr, nullptr, cls.hInstance, this);
        if (!window_) return false;
        SendMessageW(window_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon_));
        if (const UINT dpi = GetDpiForWindow(window_); dpi && dpi != dpi_) {
            dpi_ = dpi;
            RECT actual{0, 0, px(580), px(content_height)};
            AdjustWindowRectExForDpi(&actual, style, FALSE, WS_EX_CONTROLPARENT, dpi_);
            const int actual_width = actual.right - actual.left;
            const int actual_height = (std::min)(actual.bottom - actual.top, work.bottom - work.top);
            SetWindowPos(window_, nullptr, work.right - actual_width - px(20),
                (std::max)(work.top, work.bottom - actual_height - px(20)),
                actual_width, actual_height, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        // Explorer may have lower integrity than the tool host. Allow only its
        // inert tray-recreation broadcast, never commands or our WM_APP channel.
        if (!taskbar_created_ || !ChangeWindowMessageFilterEx(window_, taskbar_created_, MSGFLT_ALLOW, nullptr)) {
            if (log_message) log_message(L"TaskbarCreated message filter initialization failed.");
            DestroyWindow(window_);
            return false;
        }
        if (book_.initialize(clock_sample()) && initialize_locale_)
            book_.set_locale(locale_);
        try { controls(); } catch (...) { DestroyWindow(window_); return false; }
        scroll_to(0);
        tray_.cbSize = sizeof(tray_);
        tray_.hWnd = window_;
        tray_.uID = 1;
        tray_.uCallbackMessage = tray_message;
        tray_.hIcon = icon_;
        wcsncpy_s(tray_.szTip, get_text(locale_, text::title, text_en::title), _TRUNCATE);
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
            case WM_SIZE:
                self->scroll_to(self->scroll_offset_); return 0;
            case WM_VSCROLL: {
                RECT client{};
                GetClientRect(window, &client);
                int position = self->scroll_offset_;
                switch (LOWORD(wparam)) {
                    case SB_LINEUP: position -= self->px(20); break;
                    case SB_LINEDOWN: position += self->px(20); break;
                    case SB_PAGEUP: position -= client.bottom; break;
                    case SB_PAGEDOWN: position += client.bottom; break;
                    case SB_TOP: position = 0; break;
                    case SB_BOTTOM: position = self->px(self->content_); break;
                    case SB_THUMBTRACK: {
                        SCROLLINFO info{};
                        info.cbSize = sizeof(info);
                        info.fMask = SIF_TRACKPOS;
                        GetScrollInfo(window, SB_VERT, &info);
                        position = info.nTrackPos;
                        break;
                    }
                    default: return 0;
                }
                self->scroll_to(position); return 0;
            }
            case WM_MOUSEWHEEL:
                self->scroll_to(self->scroll_offset_ - GET_WHEEL_DELTA_WPARAM(wparam) * self->px(60) / WHEEL_DELTA);
                return 0;
            case WM_COMMAND:
                if (HIWORD(wparam) == EN_SETFOCUS || HIWORD(wparam) == CBN_SETFOCUS || HIWORD(wparam) == BN_SETFOCUS)
                    self->reveal(reinterpret_cast<HWND>(lparam));
                if (LOWORD(wparam) == duration_id && (HIWORD(wparam) == EN_SETFOCUS || HIWORD(wparam) == EN_KILLFOCUS)) InvalidateRect(window, nullptr, TRUE);
                if (LOWORD(wparam) == duration_id && HIWORD(wparam) == EN_CHANGE) {
                    self->preview();
                }
                else if (LOWORD(wparam) == action_id && HIWORD(wparam) == CBN_SELCHANGE) { self->sync_force(); self->preview(); }
                else if (LOWORD(wparam) == force_id && HIWORD(wparam) == BN_CLICKED) self->confirm_force();
                else if (LOWORD(wparam) == kind_id && HIWORD(wparam) == CBN_SELCHANGE) self->show_kind();
                else if (HIWORD(wparam) == BN_CLICKED) self->command(LOWORD(wparam));
                return 0;
            case WM_NOTIFY: {
                const auto* header = reinterpret_cast<NMHDR*>(lparam);
                if (header->code == NM_SETFOCUS) self->reveal(header->hwndFrom);
                // Rows: rounded accent-tinted selection and subtle hover, painted before the text.
                if (!self->high_contrast_ && self->list_ && header->code == NM_CUSTOMDRAW && header->hwndFrom == self->list_) {
                    auto* rows = reinterpret_cast<NMLVCUSTOMDRAW*>(lparam);
                    if (rows->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
                    if (rows->nmcd.dwDrawStage == CDDS_POSTPAINT) {
                        // Some Windows 11 builds draw column separators in the empty area; cover it.
                        RECT empty{}, header{};
                        GetClientRect(self->list_, &empty);
                        GetWindowRect(ListView_GetHeader(self->list_), &header);
                        empty.top = header.bottom - header.top;
                        if (const int count = ListView_GetItemCount(self->list_); count > 0) {
                            RECT last{};
                            if (ListView_GetItemRect(self->list_, count - 1, &last, LVIR_BOUNDS))
                                empty.top = (std::max)(empty.top, static_cast<LONG>(last.bottom));
                        }
                        if (empty.top < empty.bottom) {
                            HBRUSH brush = CreateSolidBrush(self->field_color());
                            FillRect(rows->nmcd.hdc, &empty, brush);
                            DeleteObject(brush);
                        }
                        return CDRF_DODEFAULT;
                    }
                    if (rows->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                        const int row = static_cast<int>(rows->nmcd.dwItemSpec);
                        const bool selected = ListView_GetItemState(self->list_, row, LVIS_SELECTED) != 0;
                        if (selected || row == self->hot_row_) {
                            RECT bounds{};
                            ListView_GetItemRect(self->list_, row, &bounds, LVIR_BOUNDS);
                            InflateRect(&bounds, -self->px(6), -self->px(2));
                            const COLORREF fill = selected ? (self->dark_ ? RGB(29, 71, 103) : RGB(205, 229, 247)) : (self->dark_ ? RGB(56, 56, 56) : RGB(240, 244, 249));
                            HBRUSH brush = CreateSolidBrush(fill);
                            HPEN pen = CreatePen(PS_SOLID, 1, fill);
                            const auto old_brush = SelectObject(rows->nmcd.hdc, brush);
                            const auto old_pen = SelectObject(rows->nmcd.hdc, pen);
                            RoundRect(rows->nmcd.hdc, bounds.left, bounds.top, bounds.right, bounds.bottom, self->px(12), self->px(12));
                            SelectObject(rows->nmcd.hdc, old_brush); SelectObject(rows->nmcd.hdc, old_pen);
                            DeleteObject(brush); DeleteObject(pen);
                        }
                        rows->clrText = self->ink_; rows->clrTextBk = CLR_NONE;
                        rows->nmcd.uItemState &= ~static_cast<UINT>(CDIS_SELECTED | CDIS_FOCUS | CDIS_HOT);
                        return CDRF_NEWFONT;
                    }
                    return CDRF_DODEFAULT;
                }
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
                self->scroll_to(self->scroll_offset_);
                return 0;
            }
            case WM_SETTINGCHANGE:
                self->colors();
                if (lparam && (lstrcmpiW(reinterpret_cast<LPCWSTR>(lparam), L"TimeZoneInformation") == 0)) {
                    self->list_signature_.clear();
                    self->book_.reconcile(clock_sample()); self->refresh();
                }
                return 0;
            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX:
                if (self->dark_ && !self->high_contrast_) {
                    SetTextColor(reinterpret_cast<HDC>(wparam), self->ink_);
                    SetBkColor(reinterpret_cast<HDC>(wparam), RGB(45, 45, 45));
                    if (!self->field_brush_) self->field_brush_ = CreateSolidBrush(RGB(45, 45, 45));
                    return reinterpret_cast<LRESULT>(self->field_brush_);
                }
                break;
            case WM_CTLCOLORSTATIC:
                SetTextColor(reinterpret_cast<HDC>(wparam), reinterpret_cast<HWND>(lparam) == self->empty_ ? self->muted_ : self->ink_);
                SetBkColor(reinterpret_cast<HDC>(wparam), self->bg_);
                return reinterpret_cast<LRESULT>(self->background_);
            case WM_ERASEBKGND: {
                RECT rect{}; GetClientRect(window, &rect);
                FillRect(reinterpret_cast<HDC>(wparam), &rect, self->background_);
                self->paint_duration_field(reinterpret_cast<HDC>(wparam)); return 1;
            }
            case WM_DRAWITEM:
                if (reinterpret_cast<DRAWITEMSTRUCT*>(lparam)->CtlType == ODT_COMBOBOX) self->draw_combo_item(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
                else self->draw_button(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam));
                return TRUE;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }
};

inline HANDLE user_mutex() {
    const auto sid = current_user_sid();
    if (sid.empty()) return nullptr;
    const std::wstring name = std::wstring(L"Global\\CrazyTime.") + (dedicated_process ? L"Mod." : L"Preview.") + sid;
    return CreateMutexW(nullptr, FALSE, name.c_str());
}

inline DWORD WINAPI ui_main(void*) {
    HANDLE instance = user_mutex();
    if (!instance) { SetEvent(ready_event); return 1; }
    const DWORD locked = WaitForSingleObject(instance, 0);
    if (locked != WAIT_OBJECT_0 && locked != WAIT_ABANDONED) {
        if (show_on_start)
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
inline bool start(bool visible, void (*logger)(const wchar_t*) = nullptr, bool dedicated = false, bool simulate = false) {
    simulation_mode = simulate;
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
