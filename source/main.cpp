#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <intrin.h>

#include <cstddef>
#include <cstdint>

static_assert(sizeof(void*) == 4, "!0fastload.asi must be built for Win32.");
extern "C" int _fltused = 0;
// Explicit string intrinsics keep these CRT shims from calling themselves under /O1.
#pragma function(memcpy, memset)
extern "C" void* __cdecl memcpy(void* dst, const void* src, std::size_t size) {
    __movsb(static_cast<unsigned char*>(dst), static_cast<const unsigned char*>(src), size);
    return dst;
}
extern "C" void* __cdecl memset(void* dst, int value, std::size_t size) {
    __stosb(static_cast<unsigned char*>(dst), static_cast<unsigned char>(value), size);
    return dst;
}

#include "launch_mode.h"
#include "patching.h"
#include "single_player.h"

namespace {

using fastload_patch::Access;
using fastload_patch::Apply;
using fastload_patch::BranchTo;
using fastload_patch::Bytes;
using fastload_patch::Call;
using fastload_patch::Equal;
using fastload_patch::ImageMemory;
using fastload_patch::kImageBase;
using fastload_patch::Patch;

constexpr std::uintptr_t kGameState = 0x00C8D4C0;
constexpr std::uintptr_t kGameWindow = 0x00C8CF88;
constexpr std::uintptr_t kForeground = 0x008D621C;
constexpr std::uintptr_t kLoadingScreenActive = 0x00BAB318;
constexpr std::uintptr_t kGetUserFiles = 0x00744FB0;
constexpr std::uintptr_t kSetDirMyDocuments = 0x00538860;
constexpr std::uintptr_t kCallPostLoading = 0x00748D15;

using StartupFn = int (__cdecl*)();
using InitScreenFn = void (__cdecl*)(int, int);
using FadeFn = void (__cdecl*)();
using AudioFn = void (__thiscall*)(void*);

StartupFn g_originalStartup;
InitScreenFn g_originalInitScreen;
FadeFn g_originalFadeIn;
FadeFn g_originalFadeOut;
AudioFn g_originalStartTune;
AudioFn g_originalPostLoading;

bool g_commonInstalled;
bool g_sampLaunch;
bool g_coldStartup;
bool g_portableCalled;
unsigned g_sampApplied;

constexpr Patch kSampPatches[] = {
    Bytes(0x747FB0, {0xC7, 0x05, 0x1C, 0x62, 0x8D, 0, 0, 0, 0, 0},
                    {0xC7, 0x05, 0x1C, 0x62, 0x8D, 0, 1, 0, 0, 0}),
    Bytes(0x748054, {0xC7, 0x05, 0x1C, 0x62, 0x8D, 0, 0, 0, 0, 0},
                    {0xC7, 0x05, 0x1C, 0x62, 0x8D, 0, 1, 0, 0, 0}),
    Bytes(0x53BC72, {0xC6, 0x05, 0x7B, 0x67, 0xBA, 0, 1},
                    {0xC6, 0x05, 0x7B, 0x67, 0xBA, 0, 0}),
    Bytes(0x561AF0, {0xC6, 0x05, 0x49, 0xCB, 0xB7, 0, 1},
                    {0xC6, 0x05, 0x49, 0xCB, 0xB7, 0, 0}),
    Bytes(0x748CBD, {0x74, 0x0B}, {0x90, 0x90}),
};
constexpr auto kSampPatchCount = sizeof(kSampPatches) / sizeof(kSampPatches[0]);

template <typename T>
volatile T& GameValue(std::uintptr_t address) {
    return *reinterpret_cast<volatile T*>(address);
}

bool SupportedHost() {
    if (reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) != kImageBase
        || !ImageMemory(kImageBase, sizeof(IMAGE_DOS_HEADER), kImageBase, Access::Read)) return false;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(kImageBase);
    const auto peOffset = static_cast<DWORD>(dos->e_lfanew);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || peOffset < sizeof(IMAGE_DOS_HEADER)
        || peOffset > fastload_patch::kPageSize - sizeof(IMAGE_NT_HEADERS32)) return false;

    const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS32*>(kImageBase + peOffset);
    if (!ImageMemory(reinterpret_cast<std::uintptr_t>(pe), sizeof(*pe), kImageBase, Access::Read)
        || pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_I386
        || pe->FileHeader.SizeOfOptionalHeader < sizeof(pe->OptionalHeader)
        || pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC
        || pe->OptionalHeader.ImageBase != kImageBase
        || pe->OptionalHeader.SizeOfImage <= kGameState - kImageBase + sizeof(LONG) - 1) return false;

    constexpr unsigned char dispatch[] = {
        0xA1, 0xC0, 0xD4, 0xC8, 0x00, 0x83, 0xF8, 0x09,
        0x0F, 0x87, 0x02, 0x03, 0, 0, 0xFF, 0x24, 0x85, 0xF8, 0x8E, 0x74, 0,
    };
    return ImageMemory(0x748A93, sizeof(dispatch), kImageBase, Access::Code)
        && Equal(0x748A93, dispatch, sizeof(dispatch))
        && ImageMemory(kGameState, sizeof(LONG), kImageBase, Access::Write)
        && ImageMemory(kGameWindow, sizeof(HWND), kImageBase, Access::Read)
        && ImageMemory(kLoadingScreenActive, 1, kImageBase, Access::Read)
        && ImageMemory(kForeground, sizeof(DWORD), kImageBase, Access::Write);
}

bool PortableGetterReady() {
    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(L"portablegta.asi"));
    fastload_patch::RelativeCall jump{};
    return module && fastload_patch::ReadBranch(kGetUserFiles, 0xE9, jump)
        && ImageMemory(fastload_patch::Target(kGetUserFiles, jump), 1, module, Access::Code);
}

void PortableOnce(bool nativeCall = false) {
    if (g_portableCalled) return;
    if (!nativeCall && !PortableGetterReady()) return;
    g_portableCalled = true;
    if (!nativeCall) reinterpret_cast<char* (__cdecl*)()>(kGetUserFiles)();
}

void ApplySampPatches() {
    if (g_sampApplied) return;

    Patch pending[kSampPatchCount]{};
    std::size_t count = 0;
    unsigned mask = 0;
    for (std::size_t i = 0; i < kSampPatchCount; ++i) {
        if (fastload_patch::Matches(kSampPatches[i], true)) continue;
        pending[count++] = kSampPatches[i];
        mask |= 1u << i;
    }
    if (count && Apply(pending, count)) g_sampApplied = mask;
}

void RestoreSampPatches() {
    Patch owned[kSampPatchCount]{};
    std::size_t count = 0;
    for (std::size_t i = 0; i < kSampPatchCount; ++i) {
        if ((g_sampApplied & (1u << i)) && fastload_patch::Matches(kSampPatches[i], true))
            owned[count++] = kSampPatches[i];
    }
    if (count && !Apply(owned, count, true)) {
        for (std::size_t i = 0; i < count; ++i) (void)Apply(&owned[i], 1, true);
    }
    const bool restoreForeground = (g_sampApplied & 3) != 0
        && fastload_patch::Matches(kSampPatches[0]) && fastload_patch::Matches(kSampPatches[1]);
    g_sampApplied = 0;
    if (!restoreForeground) return;

    using ForegroundWindowFn = HWND (WINAPI*)();
    const HMODULE user32 = GetModuleHandleW(L"user32.dll");
    const auto findForeground = user32 ? reinterpret_cast<ForegroundWindowFn>(
        reinterpret_cast<std::uintptr_t>(GetProcAddress(user32, "GetForegroundWindow"))) : nullptr;
    if (findForeground && GameValue<HWND>(kGameWindow))
        GameValue<DWORD>(kForeground) = findForeground() == GameValue<HWND>(kGameWindow) ? 1u : 0u;
}

void __fastcall PostLoading(void* audio, void*);

bool OwnsPostLoading() {
    return BranchTo(kCallPostLoading, 0xE8, reinterpret_cast<std::uintptr_t>(&PostLoading));
}

int __cdecl BeforeStartup() {
    // The native directory setup already calls the getter when PortableGTA is ready.
    const bool ready = g_commonInstalled && PortableGetterReady();
    const bool nativeGetter = ready && BranchTo(kSetDirMyDocuments, 0xE8, kGetUserFiles);
    const int result = g_originalStartup();
    if (!g_commonInstalled) return result;

    if (ready) PortableOnce(nativeGetter);
    auto& state = GameValue<LONG>(kGameState);
    if (state != 0 && state != 5) return result;

    g_coldStartup = OwnsPostLoading();
    g_sampLaunch = g_sampLaunch || fastload_launch::SampModuleLoaded();
    if (g_sampLaunch) {
        if (g_coldStartup) {
            ApplySampPatches();
            if (g_sampApplied) GameValue<DWORD>(kForeground) = 1;
        }
    } else {
        // The executable has already performed its own reset. SA:MP still owns its signature.
        InterlockedCompareExchange(&state, 5, 0);
    }
    return result;
}

void __cdecl InitScreen(int start, int dontFade) {
    if (!g_coldStartup) g_originalInitScreen(start, dontFade);
}

void __cdecl FadeIn() {
    if (!g_coldStartup || GameValue<unsigned char>(kLoadingScreenActive)) g_originalFadeIn();
}

void __cdecl FadeOut() {
    if (g_coldStartup) PortableOnce();
    if (!g_coldStartup || GameValue<unsigned char>(kLoadingScreenActive)) g_originalFadeOut();
}

void __fastcall StartTune(void* audio, void*) {
    if (!g_coldStartup) {
        g_originalStartTune(audio);
        return;
    }

    PortableOnce();
    g_coldStartup = false;
    if (g_sampApplied && !OwnsPostLoading()) RestoreSampPatches();
}

void __fastcall PostLoading(void* audio, void*) {
    g_originalPostLoading(audio);
    if (g_commonInstalled) PortableOnce();
    g_coldStartup = false;
    if (g_sampApplied) RestoreSampPatches();
}

bool InstallCommonPatches() {
    Patch patches[6]{};
    return Call(patches[0], 0x748995, kSetDirMyDocuments, reinterpret_cast<std::uintptr_t>(&BeforeStartup), g_originalStartup)
        && Call(patches[1], 0x748C23, 0x5902B0, reinterpret_cast<std::uintptr_t>(&InitScreen), g_originalInitScreen)
        && Call(patches[2], 0x748C2B, 0x590860, reinterpret_cast<std::uintptr_t>(&FadeIn), g_originalFadeIn)
        && Call(patches[3], 0x748C9A, 0x590990, reinterpret_cast<std::uintptr_t>(&FadeOut), g_originalFadeOut)
        && Call(patches[4], 0x748CF6, 0x507410, reinterpret_cast<std::uintptr_t>(&StartTune), g_originalStartTune)
        && Call(patches[5], kCallPostLoading, 0x5078F0, reinterpret_cast<std::uintptr_t>(&PostLoading), g_originalPostLoading)
        && Apply(patches, 6);
}

void InstallSinglePlayerPatches() {
    using namespace fastload_sp;
    if (!ImageMemory(kMenuManager + kDeactivate, kSlot - kDeactivate + 1, kImageBase, Access::Write)
        || !ImageMemory(kMenuManager + kLoadRequest, 1, kImageBase, Access::Write)
        || !ImageMemory(kMissionPack, 1, kImageBase, Access::Write)
        || !ImageMemory(kUserPause, 1, kImageBase, Access::Write)
        || !ImageMemory(kSaveStatus, sizeof(DWORD), kImageBase, Access::Write)
        || !ImageMemory(fastload_sp::kGetUserFiles, 5, kImageBase, Access::Code)) return;

    Patch patches[4]{};
    const HMODULE modloader = GetModuleHandleW(L"modloader.asi");
    g_enabled =
        Call(patches[0], 0x53ECCB, 0x53E770, reinterpret_cast<std::uintptr_t>(&FrontendIdleSP),
             g_originalFrontend, modloader)
        && Call(patches[1], 0x53E80E, 0x619450, reinterpret_cast<std::uintptr_t>(&CameraBeginSP), g_originalCamera)
        && Call(patches[2], 0x578EF2, 0x5D1380, reinterpret_cast<std::uintptr_t>(&CheckSlotSP), g_originalCheckSlot)
        && Call(patches[3], 0x578F37, 0x576AE0, reinterpret_cast<std::uintptr_t>(&LoadErrorSP), g_originalLoadError)
        && Apply(patches, 4);
}

void Init(HMODULE self) {
    if (!SupportedHost() || GameValue<HWND>(kGameWindow)) return;
    const LONG state = GameValue<LONG>(kGameState);
    if (state != 0 && state != 5) return;

    // GTA callsites retain our targets for the duration of the process.
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(self), &pinned)) return;

    g_sampLaunch = fastload_launch::LooksLikeSamp();
    g_commonInstalled = InstallCommonPatches();
    if (!g_sampLaunch) InstallSinglePlayerPatches();
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        Init(module);
    }
    return TRUE;
}
