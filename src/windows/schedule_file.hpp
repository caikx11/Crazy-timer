#pragma once
#include "../schedule_codec.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <utility>

namespace power_timer {
// The host creates the parent directory and serializes access. Invalid/Unavailable load
// must gate writes in the engine, preserving corrupt files for explicit recovery.
// Temp content is flushed before a same-volume replacement. Flush/write-through cannot
// guarantee survival of power loss across every filesystem/device write cache. A crash
// before replacement may leave an orphan temp file; load never promotes it to live data.
class FileScheduleStorage final : public ScheduleStorage {
public:
    explicit FileScheduleStorage(std::wstring path) : path_(std::move(path)) {}
    LoadResult load() override {
        try {
            Handle file(CreateFileW(path_.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
            if (!file.valid()) {
                return {GetLastError() == ERROR_FILE_NOT_FOUND ? LoadStatus::Missing : LoadStatus::Unavailable, {}};
            }
            LARGE_INTEGER size{};
            if (!GetFileSizeEx(file.get(), &size)) return {LoadStatus::Unavailable, {}};
            if (size.QuadPart <= 0 || size.QuadPart > static_cast<LONGLONG>(max_schedule_file_bytes))
                return {LoadStatus::Invalid, {}};
            std::string data(static_cast<std::size_t>(size.QuadPart), '\0');
            DWORD read = 0;
            if (!ReadFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &read, nullptr))
                return {LoadStatus::Unavailable, {}};
            if (read != data.size()) return {LoadStatus::Invalid, {}};
            auto snapshot = decode_snapshot(data);
            return snapshot ? LoadResult{LoadStatus::Loaded, std::move(*snapshot)} : LoadResult{LoadStatus::Invalid, {}};
        } catch (...) { return {LoadStatus::Unavailable, {}}; }
    }
    bool save(const ScheduleSnapshot& snapshot) override {
        try {
            const auto data = encode_snapshot(snapshot);
            if (data.empty() || data.size() > max_schedule_file_bytes) return false;
            static std::atomic<std::uint64_t> sequence{0};
            for (unsigned attempt = 0; attempt < 16; ++attempt) {
                Temporary temporary(path_ + L".tmp-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                    std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence.fetch_add(1)));
                Handle file(CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
                if (!file.valid()) {
                    if (GetLastError() == ERROR_FILE_EXISTS || GetLastError() == ERROR_ALREADY_EXISTS) continue;
                    return false;
                }
                temporary.owned = true;
                DWORD written = 0;
                if (!WriteFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &written, nullptr) ||
                    written != data.size() || !FlushFileBuffers(file.get())) return false;
                if (!file.close()) return false;
                // Same directory/volume; COPY_ALLOWED is deliberately absent.
                // A failed/ambiguous commit returns false, so callers can block effects.
                if (!MoveFileExW(temporary.path.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                    return false;
                temporary.owned = false;
                return true;
            }
            return false;
        } catch (...) { return false; }
    }
private:
    class Handle {
    public:
        explicit Handle(HANDLE value) : value_(value) {}
        ~Handle() { if (valid()) CloseHandle(value_); }
        bool valid() const { return value_ != INVALID_HANDLE_VALUE; }
        HANDLE get() const { return value_; }
        bool close() { const auto value = value_; value_ = INVALID_HANDLE_VALUE; return CloseHandle(value) != 0; }
    private:
        HANDLE value_;
    };
    struct Temporary {
        explicit Temporary(std::wstring name) : path(std::move(name)) {}
        ~Temporary() { if (owned) DeleteFileW(path.c_str()); }
        std::wstring path;
        bool owned = false;
    };
    std::wstring path_;
};
} // namespace power_timer

