#pragma once

#include <windows.h>

#include <cstddef>
#include <cstdint>

#include "launch_mode.h"

namespace fastload_sp {

using FrontendFn = void (__cdecl*)();
using CameraFn = void* (__cdecl*)(void*);
using CheckSlotFn = bool (__cdecl*)(int);
using LoadErrorFn = void* (__thiscall*)(void*, int, const char*, const char*);

inline FrontendFn g_originalFrontend{};
inline CameraFn g_originalCamera{};
inline CheckSlotFn g_originalCheckSlot{};
inline LoadErrorFn g_originalLoadError{};

constexpr int kNoSlot = -1;
constexpr int kSlotCount = 8;
constexpr char kSlotFile[] = "!0fastload_lastslot.bin";

struct SlotFilePath {
    char path[MAX_PATH];
    std::size_t prefixLength;
};

inline bool MakeSlotPath(const char* base, SlotFilePath& result) {
    if (!base) return false;
    constexpr std::size_t maxBase = MAX_PATH - sizeof(kSlotFile);
    std::size_t length = 0;
    while (length < maxBase && base[length]) ++length;
    if (length == 0 || base[length]) return false;
    const bool needsSlash = base[length - 1] != '\\' && base[length - 1] != '/';
    if (needsSlash && length == maxBase) return false;

    for (std::size_t i = 0; i < length; ++i) result.path[i] = base[i];
    if (needsSlash) result.path[length++] = '\\';
    result.prefixLength = length;
    for (char c : kSlotFile) result.path[length++] = c;
    return true;
}

inline bool ValidSlot(int slot) {
    return slot >= 0 && slot < kSlotCount;
}

inline int ReadSlot(const SlotFilePath& path) {
    HANDLE file = CreateFileA(path.path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return kNoSlot;
    unsigned char bytes[2];
    DWORD size = 0;
    const BOOL ok = ReadFile(file, bytes, sizeof(bytes), &size, nullptr);
    CloseHandle(file);
    return ok && size == 1 && ValidSlot(bytes[0]) ? bytes[0] : kNoSlot;
}

inline void FormatHex(char* output, DWORD value) {
    constexpr char digits[] = "0123456789abcdef";
    for (int i = 7; i >= 0; --i) {
        output[i] = digits[value & 15];
        value >>= 4;
    }
}

inline bool WriteSlot(const SlotFilePath& path, int slot) {
    if (!ValidSlot(slot)) return false;
    constexpr char tempName[] = "!0fl.00000000.00000000";
    static_assert(sizeof(tempName) <= sizeof(kSlotFile));
    static LONG sequence;
    char temporary[MAX_PATH];
    for (std::size_t i = 0; i < path.prefixLength; ++i) temporary[i] = path.path[i];
    char* name = temporary + path.prefixLength;
    for (std::size_t i = 0; i < sizeof(tempName); ++i) name[i] = tempName[i];
    FormatHex(name + 5, GetCurrentProcessId());

    HANDLE file = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 3; ++attempt) {
        FormatHex(name + 14, static_cast<DWORD>(InterlockedIncrement(&sequence)));
        file = CreateFileA(temporary, GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_FILE_EXISTS) return false;
    }
    if (file == INVALID_HANDLE_VALUE) return false;

    const auto value = static_cast<unsigned char>(slot);
    DWORD written = 0;
    const BOOL ok = WriteFile(file, &value, sizeof(value), &written, nullptr);
    const BOOL closed = CloseHandle(file);
    if (ok && closed && written == sizeof(value)) {
        for (int attempt = 0; attempt < 3; ++attempt) {
            if (MoveFileExA(temporary, path.path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            const DWORD error = GetLastError();
            // Windows can report ACCESS_DENIED during a concurrent replacement.
            if (attempt == 2 || (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION
                && error != ERROR_ACCESS_DENIED)) break;
            Sleep(1);
        }
    }
    DeleteFileA(temporary);
    return false;
}

constexpr std::uintptr_t kGetUserFiles = 0x00744FB0;
constexpr std::uintptr_t kMenuManager = 0x00BA6748;
constexpr std::uintptr_t kMissionPack = 0x00B72910;
constexpr std::uintptr_t kUserPause = 0x00B7CB49;
constexpr std::uintptr_t kSaveStatus = 0x00C17034;
constexpr std::uintptr_t kGameState = 0x00C8D4C0;

constexpr DWORD kFrontendState = 7;
constexpr DWORD kPlayingState = 9;
constexpr unsigned char kLoadPage = 13;
constexpr int kStartupTicks = 2;
constexpr int kMaxHiddenTicks = 16;

enum MenuOffset : std::size_t {
    kDeactivate = 0x32,
    kActive = 0x5C,
    kLoading = 0x60,
    kPage = 0x15D,
    kSlot = 0x15F,
    kLoadRequest = 0x1B3C,
};

enum class LoadResult : unsigned char { Pending, Accepted, Rejected, Restored, Superseded };

struct AutoLoadAttempt {
    int slot;
    unsigned char previousPage;
    unsigned char previousSlot;
    unsigned char previousMissionPack;
    unsigned char previousUserPause;
    DWORD previousSaveStatus;
    LoadResult result;
};

inline AutoLoadAttempt* g_autoLoad{};
inline bool g_enabled{};
inline bool g_hideCamera{};
inline bool g_sampDetected{};
inline int g_startupTicks{};
inline int g_autoloadHideTicks{};

inline bool IsSampNow() {
    if (!g_sampDetected) g_sampDetected = fastload_launch::SampModuleLoaded();
    return g_sampDetected;
}

inline volatile unsigned char* Menu() {
    return reinterpret_cast<volatile unsigned char*>(kMenuManager);
}

inline volatile unsigned char& MissionPack() {
    return *reinterpret_cast<volatile unsigned char*>(kMissionPack);
}

inline volatile unsigned char& UserPause() {
    return *reinterpret_cast<volatile unsigned char*>(kUserPause);
}

inline volatile DWORD& SaveStatus() {
    return *reinterpret_cast<volatile DWORD*>(kSaveStatus);
}

inline bool CurrentSlotPath(SlotFilePath& path) {
    using GetUserFilesFn = char* (__cdecl*)();
    if (IsSampNow()) return false;
    const char* base = reinterpret_cast<GetUserFilesFn>(kGetUserFiles)();
    return !IsSampNow() && MakeSlotPath(base, path);
}

inline void RestoreAutoLoad(AutoLoadAttempt& attempt) {
    auto* menu = Menu();
    if (menu[kPage] == kLoadPage) menu[kPage] = attempt.previousPage;
    if (menu[kSlot] == attempt.slot) menu[kSlot] = attempt.previousSlot;
    // Only the stock load page owns this flag and its validator consumes it.
    menu[kLoadRequest] = 0;
    if (MissionPack() == 0) MissionPack() = attempt.previousMissionPack;
    if (UserPause() == 0) UserPause() = attempt.previousUserPause;
    if (attempt.result == LoadResult::Rejected) SaveStatus() = attempt.previousSaveStatus;
    attempt.result = LoadResult::Restored;
}

inline void* __cdecl CameraBeginSP(void* camera) {
    if (g_enabled && g_hideCamera && !IsSampNow()) return nullptr;
    return g_originalCamera(camera);
}

inline bool __cdecl CheckSlotSP(int slot) {
    if (!g_enabled) return g_originalCheckSlot(slot);
    if (g_autoLoad && g_autoLoad->result == LoadResult::Pending) {
        if (g_autoLoad->slot != slot) {
            const bool accepted = g_originalCheckSlot(slot);
            if (accepted) g_autoLoad->result = LoadResult::Superseded;
            return accepted;
        }
        const bool accepted = !IsSampNow() && g_originalCheckSlot(slot) && !IsSampNow();
        g_autoLoad->result = accepted ? LoadResult::Accepted : LoadResult::Rejected;
        return accepted;
    }

    const bool accepted = g_originalCheckSlot(slot);
    if (accepted && !g_autoLoad && ValidSlot(slot)) {
        SlotFilePath path;
        if (CurrentSlotPath(path)) (void)WriteSlot(path, slot);
    }
    return accepted;
}

inline void* __fastcall LoadErrorSP(void* menu, void*, int page, const char* title, const char* text) {
    // This call is reached immediately after the stock validator rejects page 13.
    if (g_enabled && g_autoLoad && g_autoLoad->result == LoadResult::Rejected && menu == Menu()) {
        RestoreAutoLoad(*g_autoLoad);
        return nullptr;
    }
    return g_originalLoadError(menu, page, title, text);
}

inline bool CanAutoLoad() {
    auto* menu = Menu();
    return menu[kActive] && !menu[kDeactivate] && !menu[kLoading] && !menu[kLoadRequest] && menu[kPage] != kLoadPage;
}

inline void __cdecl FrontendIdleSP() {
    if (!g_enabled || (g_startupTicks == kStartupTicks && g_autoloadHideTicks == 0) || g_hideCamera || IsSampNow()) {
        g_originalFrontend();
        return;
    }

    const DWORD state = *reinterpret_cast<volatile const DWORD*>(kGameState);
    if (state != kFrontendState) {
        if (state >= kPlayingState) {
            g_startupTicks = kStartupTicks;
            g_autoloadHideTicks = 0;
        }
        g_originalFrontend();
        return;
    }

    const bool tryAutoload = g_startupTicks == kStartupTicks - 1;
    if (g_startupTicks == kStartupTicks) {
        --g_autoloadHideTicks;
    } else {
        ++g_startupTicks;
    }

    AutoLoadAttempt attempt{};
    SlotFilePath path;
    if (tryAutoload && CanAutoLoad() && CurrentSlotPath(path)) {
        const int slot = ReadSlot(path);
        if (slot != kNoSlot && !IsSampNow() && CanAutoLoad()) {
            auto* menu = Menu();
            attempt = {slot, menu[kPage], menu[kSlot], MissionPack(), UserPause(), SaveStatus(), LoadResult::Pending};
            menu[kSlot] = static_cast<unsigned char>(slot);
            menu[kPage] = kLoadPage;
            menu[kLoadRequest] = 1;
            MissionPack() = 0;
            UserPause() = 0;
            g_autoLoad = &attempt;
        }
    }

    g_hideCamera = true;
    g_originalFrontend();
    g_hideCamera = false;
    if (g_autoLoad) {
        if (attempt.result == LoadResult::Accepted) {
            g_autoloadHideTicks = kMaxHiddenTicks - kStartupTicks;
        } else if (attempt.result != LoadResult::Restored && attempt.result != LoadResult::Superseded) {
            RestoreAutoLoad(attempt);
        }
        g_autoLoad = nullptr;
    }
}

} // namespace fastload_sp
