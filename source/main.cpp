#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <limits>

static_assert(sizeof(void*) == 4, "!0fastload.asi must be built for Win32.");
extern "C" int _fltused = 0;
#pragma function(memcpy, memset)
extern "C" void* __cdecl memcpy(void* dst, const void* src, std::size_t size) {
    auto* out = static_cast<unsigned char*>(dst);
    const auto* in = static_cast<const unsigned char*>(src);
    for (std::size_t i = 0; i < size; ++i) out[i] = in[i];
    return dst;
}
extern "C" void* __cdecl memset(void* dst, int value, std::size_t size) {
    auto* out = static_cast<unsigned char*>(dst);
    for (std::size_t i = 0; i < size; ++i) out[i] = static_cast<unsigned char>(value);
    return dst;
}

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
constexpr std::uintptr_t kPortableGtaUserFilesGetterAddress = 0x00744FB0;

constexpr DWORD kDisplayedSplashIndexAddress = 0x008D093C;
constexpr DWORD kFirstLoadscreenSplashAddress = 0x00BAB31E;
constexpr DWORD kTimeSinceLastSplashAddress = 0x00BAB340;

float g_loadscreenTime = 0.1f;
int g_previousDisplayedSplash = 1;
constexpr std::uintptr_t kPrevDisplayedSplashOperandAddresses[] = {
    kPrevDisplayedSplashOperandAddress1,
    kPrevDisplayedSplashOperandAddress2,
    kPrevDisplayedSplashOperandAddress3,
    kPrevDisplayedSplashOperandAddress4,
    kPrevDisplayedSplashOperandAddress5,
};

#pragma pack(push, 1)
struct RelativeBranchPatch {
    std::uint8_t opcode;
    std::int32_t relative;
};
#pragma pack(pop)
void* TinyMemset(void* dst, int value, std::size_t size) {
    auto* out = static_cast<unsigned char*>(dst);
    for (std::size_t i = 0; i < size; ++i) out[i] = static_cast<unsigned char>(value);
    return dst;
}
void* TinyMemcpy(void* dst, const void* src, std::size_t size) {
    auto* out = static_cast<unsigned char*>(dst);
    const auto* in = static_cast<const unsigned char*>(src);
    for (std::size_t i = 0; i < size; ++i) out[i] = in[i];
    return dst;
}
std::size_t TinyStrlen(const char* s) {
    std::size_t n = 0;
    while (s && s[n]) ++n;
    return n;
}
bool PatchWrite(std::uintptr_t address, const void* data, std::size_t size, int fill = -1) {
    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
    if (fill >= 0) {
        TinyMemset(reinterpret_cast<void*>(address), fill, size);
    } else {
        TinyMemcpy(reinterpret_cast<void*>(address), data, size);
    }
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
    DWORD restoreProtect = 0;
    VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &restoreProtect);
    return true;
}

template <typename T>
bool WriteValue(std::uintptr_t address, const T& value) { return PatchWrite(address, &value, sizeof(value)); }

bool Nop(std::uintptr_t address, std::size_t size) { return PatchWrite(address, nullptr, size, 0x90); }

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
    return PatchWrite(address, &patch, sizeof(patch));
}

template <typename T>
bool WritePointerOperand(std::uintptr_t address, T* pointer) {
    const auto rawPointer = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
    return WriteValue(address, rawPointer);
}

DWORD ReadGameState() { return *reinterpret_cast<volatile const DWORD*>(kGtaLoadStateAddress); }

inline bool IsBoundary(char c) {
    return c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"' || c == '\'';
}
inline char LowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c; }
bool HasToken(const char* commandLine, const char* token) {
    if (!commandLine || !token || !*token) return false;
    const std::size_t n = TinyStrlen(token);
    for (const char* p = commandLine; *p; ++p) {
        if (p != commandLine && !IsBoundary(*(p - 1))) continue;
        std::size_t i = 0;
        while (i < n && p[i] && LowerAscii(p[i]) == token[i]) ++i;
        if (i == n && IsBoundary(p[n])) return true;
    }
    return false;
}
bool IsSampRuntime() {
    const char* cmd = GetCommandLineA();
    return GetModuleHandleA("samp.dll") != nullptr || HasToken(cmd, "-c") || HasToken(cmd, "-h") || HasToken(cmd, "-p");
}

bool IsPortableGtaLoaded() {
    return GetModuleHandleA("portablegta.asi") != nullptr
        || GetModuleHandleA("PortableGTA.asi") != nullptr;
}

void EnsurePortableGtaUserfilesInitialized() {
    if (!IsPortableGtaLoaded()) {
        return;
    }

    using GetUserFilesDirFn = char* (__cdecl*)();
    const auto getUserFilesDir = reinterpret_cast<GetUserFilesDirFn>(kPortableGtaUserFilesGetterAddress);
    // portablegta patches this entry point, so call through once after
    // fast-load edits to ensure userfiles path setup still happens.
    (void)getUserFilesDir();
}

void __cdecl IncreaseDisplayedSplash() {
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
}

void __cdecl SimulateCopyrightScreen() {
    auto* currentSplash = reinterpret_cast<volatile int*>(kDisplayedSplashIndexAddress);
    auto* timeSinceLastSplash = reinterpret_cast<volatile float*>(kTimeSinceLastSplashAddress);
    auto* firstLoadscreenSplash = reinterpret_cast<volatile unsigned char*>(kFirstLoadscreenSplashAddress);

    *currentSplash = 0;
    *timeSinceLastSplash -= 1000.0f;
    *firstLoadscreenSplash = 1;
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
    ok &= PatchWrite(kForceGameStateWriteAddress, kForceGameStateWritePatch, sizeof(kForceGameStateWritePatch));
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
    for (const std::uintptr_t address : kPrevDisplayedSplashOperandAddresses) {
        ok &= WritePointerOperand(address, &g_previousDisplayedSplash);
    }
    ok &= WritePointerOperand(kLoadscreenTimeOperandAddress, &g_loadscreenTime);
    ok &= Nop(kDisableLoadingBarRenderAddress, 5);
    ok &= PatchWrite(
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
    if (ReadGameState() >= 9) {
        return;
    }

    const bool patchesApplied = IsSampRuntime()
        ? ApplySampPatches()
        : ApplySinglePlayerPatches();

    if (patchesApplied) {
        EnsurePortableGtaUserfilesInitialized();
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
