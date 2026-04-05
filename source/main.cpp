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

constexpr std::uintptr_t kForceForegroundStateWriteValueAddress = 0x0074805A;
constexpr std::uintptr_t kAllowFrontendIdleWhenMinimizedAddress = 0x00748CBD;
constexpr std::uintptr_t kDisableMenuOnFocusLossValueAddress = 0x0053BC78;
constexpr std::uintptr_t kKeepGameUnpausedWhenPausedValueAddress = 0x00561AF6;

constexpr std::uintptr_t kSkipFadeRenderAddress = 0x00590AE4;
constexpr std::uintptr_t kSkipFadeRenderTargetAddress = 0x00590C9E;
constexpr std::uintptr_t kIncreaseDisplayedSplashCallAddress = 0x00590ADE;
constexpr std::uintptr_t kPrevDisplayedSplashOperandAddress1 = 0x00590044;
constexpr std::uintptr_t kPrevDisplayedSplashOperandAddress2 = 0x0059059D;
constexpr std::uintptr_t kPrevDisplayedSplashOperandAddress3 = 0x00590B23;
constexpr std::uintptr_t kPrevDisplayedSplashOperandAddress4 = 0x00590BF7;
constexpr std::uintptr_t kPrevDisplayedSplashOperandAddress5 = 0x00590C6A;
constexpr std::uintptr_t kDisableLoadingBarRenderAddress = 0x005905B4;
constexpr std::uintptr_t kDisableLoadingScreenRenderAddress = 0x00590D9F;
constexpr std::uintptr_t kLoadscreenTimeOperandAddress = 0x00590DA6;

constexpr DWORD kDisplayedSplashIndexAddress = 0x008D093C;
constexpr DWORD kFirstLoadscreenSplashAddress = 0x00BAB31E;
constexpr DWORD kTimeSinceLastSplashAddress = 0x00BAB340;

float g_loadscreenTime = 0.1f;
int g_previousDisplayedSplash = 1;

enum class RuntimeMode {
    SinglePlayer,
    Samp,
};

#pragma pack(push, 1)
struct RelativeBranchPatch {
    std::uint8_t opcode;
    std::int32_t relative;
};
#pragma pack(pop)

#if defined(_DEBUG)
#define FASTLOAD_DEBUG_LOG(message) OutputDebugStringA(message)
#else
#define FASTLOAD_DEBUG_LOG(message) do { } while (false)
#endif

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

bool IsBoundaryCharacter(char value) {
    return value == '\0'
        || value == ' '
        || value == '\t'
        || value == '\r'
        || value == '\n'
        || value == '"'
        || value == '\'';
}

char ToLowerAscii(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

bool EqualsIgnoreCaseAscii(char lhs, char rhs) {
    return ToLowerAscii(lhs) == ToLowerAscii(rhs);
}

bool CommandLineHasToken(const char* commandLine, const char* token) {
    if (commandLine == nullptr || token == nullptr || *token == '\0') {
        return false;
    }

    const std::size_t tokenLength = std::strlen(token);
    for (const char* current = commandLine; *current != '\0'; ++current) {
        if (current != commandLine && !IsBoundaryCharacter(*(current - 1))) {
            continue;
        }

        std::size_t matched = 0;
        while (matched < tokenLength
            && current[matched] != '\0'
            && EqualsIgnoreCaseAscii(current[matched], token[matched])) {
            ++matched;
        }

        if (matched == tokenLength && IsBoundaryCharacter(current[tokenLength])) {
            return true;
        }
    }

    return false;
}

bool HasSampLaunchArguments() {
    const char* const commandLine = GetCommandLineA();
    return CommandLineHasToken(commandLine, "-c")
        || CommandLineHasToken(commandLine, "-h")
        || CommandLineHasToken(commandLine, "-p");
}

RuntimeMode DetectRuntimeMode() {
    if (GetModuleHandleA("samp.dll") != nullptr || HasSampLaunchArguments()) {
        return RuntimeMode::Samp;
    }
    return RuntimeMode::SinglePlayer;
}

void __cdecl IncreaseDisplayedSplash() {
    __try {
        auto* currentSplash = reinterpret_cast<volatile int*>(kDisplayedSplashIndexAddress);
        int nextSplash = *currentSplash;
        // Mirror imfast's stored "previous" splash index so the engine's fade code
        // can keep using a stable source even when the splash sequence loops.
        g_previousDisplayedSplash = nextSplash + 1;
        ++nextSplash;
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

bool ApplySinglePlayerPatches() {
    static constexpr std::uint8_t kForceGameStateWritePatch[] = {
        0xC7, 0x05, 0xC0, 0xD4, 0xC8, 0x00, 0x05, 0x00, 0x00, 0x00,
    };
    static constexpr std::uint8_t kDisableLoadingScreenRenderPatch[] = {
        0xC3, 0x90, 0x90, 0x90, 0x90,
    };

    bool ok = true;
    ok &= Nop(kDisableInitialGameStateResetAddress, 6);
    ok &= WriteValue(kGtaLoadStateAddress, kTargetGameState);
    ok &= Nop(kDisableLoadingAudioAddress, 5);
    ok &= Nop(kForceGameStateWriteAddress, 6);
    ok &= WriteBytes(kForceGameStateWriteAddress, kForceGameStateWritePatch, sizeof(kForceGameStateWritePatch));
    ok &= Nop(kDisablePreCopyrightCallAddress, 5);
    ok &= Nop(kDisableCopyrightJumpAddress, 5);
    ok &= WriteRelativeBranch(
        kCopyrightCallAddress,
        reinterpret_cast<std::uintptr_t>(&SimulateCopyrightScreen),
        0xE8);

    ok &= WriteRelativeBranch(kSkipFadeRenderAddress, kSkipFadeRenderTargetAddress, 0xE9);
    ok &= WriteRelativeBranch(
        kIncreaseDisplayedSplashCallAddress,
        reinterpret_cast<std::uintptr_t>(&IncreaseDisplayedSplash),
        0xE8);
    ok &= Nop(kIncreaseDisplayedSplashCallAddress + 5, 1);
    ok &= WritePointerOperand(kPrevDisplayedSplashOperandAddress1, &g_previousDisplayedSplash);
    ok &= WritePointerOperand(kPrevDisplayedSplashOperandAddress2, &g_previousDisplayedSplash);
    ok &= WritePointerOperand(kPrevDisplayedSplashOperandAddress3, &g_previousDisplayedSplash);
    ok &= WritePointerOperand(kPrevDisplayedSplashOperandAddress4, &g_previousDisplayedSplash);
    ok &= WritePointerOperand(kPrevDisplayedSplashOperandAddress5, &g_previousDisplayedSplash);
    ok &= WritePointerOperand(kLoadscreenTimeOperandAddress, &g_loadscreenTime);
    ok &= Nop(kDisableLoadingBarRenderAddress, 5);
    ok &= WriteBytes(
        kDisableLoadingScreenRenderAddress,
        kDisableLoadingScreenRenderPatch,
        sizeof(kDisableLoadingScreenRenderPatch));

    return ok;
}

bool ApplySampPatches() {
    bool ok = ApplySinglePlayerPatches();

    ok &= WriteValue<std::uint8_t>(kForceForegroundStateWriteValueAddress, static_cast<std::uint8_t>(1));
    ok &= WriteValue<std::uint8_t>(kDisableMenuOnFocusLossValueAddress, static_cast<std::uint8_t>(0));
    ok &= WriteValue<std::uint8_t>(kKeepGameUnpausedWhenPausedValueAddress, static_cast<std::uint8_t>(0));
    ok &= Nop(kAllowFrontendIdleWhenMinimizedAddress, 2);

    return ok;
}

void InitializePlugin() {
    DWORD gameState = 0;
    if (!ReadGameState(gameState) || gameState >= 9) {
        FASTLOAD_DEBUG_LOG("[!0fastload] Startup patch skipped.\n");
        return;
    }

    const RuntimeMode runtimeMode = DetectRuntimeMode();

    const bool patchesApplied = (runtimeMode == RuntimeMode::Samp)
        ? ApplySampPatches()
        : ApplySinglePlayerPatches();

    if (patchesApplied) {
        FASTLOAD_DEBUG_LOG((runtimeMode == RuntimeMode::Samp)
            ? "[!0fastload] Applied SA-MP-safe startup patches.\n"
            : "[!0fastload] Applied singleplayer fast-load patches.\n");
    } else {
        FASTLOAD_DEBUG_LOG("[!0fastload] Startup patches applied with errors.\n");
    }
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        InitializePlugin();
    }

    return TRUE;
}
