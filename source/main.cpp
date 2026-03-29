#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

static_assert(sizeof(void*) == 4, "!0fastload.asi must be built for Win32.");

namespace {

constexpr DWORD kGtaLoadStateAddress = 0x00C8D4C0;
constexpr std::uint32_t kTargetGameState = 5;

constexpr std::uintptr_t kDisableInitialGameStateResetAddress = 0x00747483;
constexpr std::uintptr_t kDisableLoadingAudioAddress = 0x00748CF6;
constexpr std::uintptr_t kCopyrightCallAddress = 0x00748C9A;
constexpr std::uintptr_t kForceGameStateWriteAddress = 0x00748AA8;
constexpr std::uintptr_t kDisablePreCopyrightCallAddress = 0x00748C23;
constexpr std::uintptr_t kDisableCopyrightJumpAddress = 0x00748C2B;

constexpr std::uintptr_t kSkipFadeRenderAddress = 0x00590AE4;
constexpr std::uintptr_t kSkipFadeRenderTargetAddress = 0x00590C9E;
constexpr std::uintptr_t kIncreaseDisplayedSplashCallAddress = 0x00590ADE;
constexpr std::uintptr_t kDisableLoadingBarRenderAddress = 0x005905B4;
constexpr std::uintptr_t kDisableLoadingScreenRenderAddress = 0x00590D9F;
constexpr std::uintptr_t kLoadscreenTimeOperandAddress = 0x00590DA6;

constexpr DWORD kDisplayedSplashIndexAddress = 0x008D093C;
constexpr DWORD kFirstLoadscreenSplashAddress = 0x00BAB31E;
constexpr DWORD kTimeSinceLastSplashAddress = 0x00BAB340;

constexpr bool kSkipLoadingScreen = true;
float g_loadscreenTime = 1.01f;

#pragma pack(push, 1)
struct RelativeBranchPatch {
    std::uint8_t opcode;
    std::int32_t relative;
};
#pragma pack(pop)

void DebugLog(const char* message) {
    OutputDebugStringA(message);
}

bool WriteBytes(std::uintptr_t address, const void* data, std::size_t size) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    std::memcpy(reinterpret_cast<void*>(address), data, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);

    DWORD restoreProtect = 0;
    VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &restoreProtect);
    return true;
}

template <typename T>
bool WriteValue(std::uintptr_t address, const T& value) {
    return WriteBytes(address, &value, sizeof(value));
}

bool Nop(std::uintptr_t address, std::size_t size) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    std::memset(reinterpret_cast<void*>(address), 0x90, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);

    DWORD restoreProtect = 0;
    VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &restoreProtect);
    return true;
}

bool WriteRelativeBranch(std::uintptr_t address, std::uintptr_t target, std::uint8_t opcode) {
    const std::int64_t relative = static_cast<std::int64_t>(target)
        - static_cast<std::int64_t>(address + sizeof(RelativeBranchPatch));
    if (relative < std::numeric_limits<std::int32_t>::min()
        || relative > std::numeric_limits<std::int32_t>::max()) {
        return false;
    }

    const RelativeBranchPatch patch{
        opcode,
        static_cast<std::int32_t>(relative),
    };
    return WriteBytes(address, &patch, sizeof(patch));
}

template <typename T>
bool WritePointerOperand(std::uintptr_t address, T* pointer) {
    const auto rawPointer = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
    return WriteValue(address, rawPointer);
}

bool ReadGameState(DWORD& value) {
    __try {
        value = *reinterpret_cast<volatile const DWORD*>(kGtaLoadStateAddress);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void __cdecl IncreaseDisplayedSplash() {
    __try {
        auto* currentSplash = reinterpret_cast<volatile int*>(kDisplayedSplashIndexAddress);
        int nextSplash = *currentSplash + 1;
        if (nextSplash >= 7) {
            nextSplash = 1;
        }
        *currentSplash = nextSplash;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

void __cdecl SimulateCopyrightScreen() {
    __try {
        auto* currentSplash = reinterpret_cast<volatile int*>(kDisplayedSplashIndexAddress);
        auto* timeSinceLastSplash = reinterpret_cast<volatile float*>(kTimeSinceLastSplashAddress);
        auto* firstLoadscreenSplash = reinterpret_cast<volatile unsigned char*>(kFirstLoadscreenSplashAddress);

        *currentSplash = 0;
        *timeSinceLastSplash -= 1000.0f;
        *firstLoadscreenSplash = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool ApplyLuaPatches(bool hasSampFuncs) {
    static constexpr std::uint8_t kForceGameStateWritePatch[] = {
        0xC7, 0x05, 0xC0, 0xD4, 0xC8, 0x00, 0x05, 0x00, 0x00, 0x00,
    };

    bool ok = true;
    ok &= Nop(kDisableInitialGameStateResetAddress, 6);
    ok &= WriteValue(kGtaLoadStateAddress, kTargetGameState);
    ok &= Nop(kDisableLoadingAudioAddress, 5);
    ok &= Nop(kForceGameStateWriteAddress, 6);
    ok &= WriteBytes(kForceGameStateWriteAddress, kForceGameStateWritePatch, sizeof(kForceGameStateWritePatch));
    ok &= Nop(kDisablePreCopyrightCallAddress, 5);

    if (hasSampFuncs) {
        ok &= Nop(kCopyrightCallAddress, 5);
    }

    ok &= Nop(kDisableCopyrightJumpAddress, 5);

    return ok;
}

bool ApplyFastLoadPatches(bool hasSampFuncs) {
    bool ok = true;

    ok &= WriteRelativeBranch(kSkipFadeRenderAddress, kSkipFadeRenderTargetAddress, 0xE9);
    ok &= WriteRelativeBranch(
        kIncreaseDisplayedSplashCallAddress,
        reinterpret_cast<std::uintptr_t>(&IncreaseDisplayedSplash),
        0xE8);
    ok &= Nop(kIncreaseDisplayedSplashCallAddress + 5, 1);
    ok &= WritePointerOperand(kLoadscreenTimeOperandAddress, &g_loadscreenTime);

    if (!hasSampFuncs) {
        ok &= WriteRelativeBranch(
            kCopyrightCallAddress,
            reinterpret_cast<std::uintptr_t>(&SimulateCopyrightScreen),
            0xE8);
    }

    if (kSkipLoadingScreen) {
        static constexpr std::uint8_t kDisableLoadingScreenRenderPatch[] = {
            0xC3, 0x90, 0x90, 0x90, 0x90,
        };

        ok &= Nop(kDisableLoadingBarRenderAddress, 5);
        ok &= WriteBytes(
            kDisableLoadingScreenRenderAddress,
            kDisableLoadingScreenRenderPatch,
            sizeof(kDisableLoadingScreenRenderPatch));
    }

    return ok;
}

DWORD WINAPI InitializePluginThread(void*) {
    DWORD gameState = 0;
    if (!ReadGameState(gameState) || gameState >= 9) {
        DebugLog("[!0fastload] Startup patch skipped.\n");
        return 0;
    }

    const bool hasSampFuncs = GetModuleHandleA("sampfuncs.asi") != nullptr;
    const bool luaPatchesApplied = ApplyLuaPatches(hasSampFuncs);
    const bool fastLoadPatchesApplied = ApplyFastLoadPatches(hasSampFuncs);

    DebugLog((luaPatchesApplied && fastLoadPatchesApplied)
        ? "[!0fastload] Fast-load patches applied.\n"
        : "[!0fastload] Fast-load patches applied with errors.\n");

    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(nullptr, 0, &InitializePluginThread, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
