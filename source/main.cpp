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

using GetUserFilesDirFn = char* (__cdecl*)();

constexpr DWORD kGtaLoadState = 0x00C8D4C0;
constexpr std::uint32_t kTargetGameState = 5;

constexpr std::uintptr_t
    kNopGameStateReset = 0x00747483,
    kNopLoadAudio = 0x00748CF6,
    kCallCopyright = 0x00748C9A,
    kPatchGameStateWrite = 0x00748AA8,
    kNopPreCopyright = 0x00748C23,
    kNopCopyrightJmp = 0x00748C2B,
    kSampForeground = 0x0074805A,
    kNopIdleWhenMin = 0x00748CBD,
    kMenuFocusLoss = 0x0053BC78,
    kUnpauseWhenPaused = 0x00561AF6,
    kCallFrontendIdle = 0x0053ECCB,
    kFrontendIdle = 0x0053E770,
    kCallRsCameraBegin = 0x0053E80E,
    kRsCameraBegin = 0x00619450,
    kJmpSkipFade = 0x00590AE4,
    kSkipFadeTarget = 0x00590C9E,
    kCallIncSplash = 0x00590ADE,
    kNopLoadBar = 0x005905B4,
    kPatchLoadScreen = 0x00590D9F,
    kOpLoadscreenTime = 0x00590DA6,
    kGetUserFiles = 0x00744FB0,
    kMenuMgr = 0x00BA6748,
    kMissionPack = 0x00B72910,
    kUserPause = 0x00B7CB49,
    kCheckSlot = 0x005D1380;

constexpr DWORD kSplashIdx = 0x008D093C, kFirstLoadSplash = 0x00BAB31E, kTimeSplash = 0x00BAB340;

constexpr std::uintptr_t kPrevSplashOps[] = {
    0x00590044, 0x0059059D, 0x00590B23, 0x00590BF7, 0x00590C6A,
};

float g_loadscreenTime = 0.1f;
int g_prevSplash = 1;

constexpr int kNoSlot = -1, kMaxSlots = 8, kHideMenuTicks = 16;
constexpr char kSlotFile[] = "!0fastload_lastslot.bin";
enum : std::size_t { M_DEACT = 0x32, M_ITEM = 0x15D, M_SAVE = 0x15F, M_START = 0x1B3C };

#pragma pack(push, 1)
struct RelBranch { std::uint8_t op; std::int32_t rel; };
#pragma pack(pop)

bool Patch(std::uintptr_t a, const void* d, std::size_t n, int fill = -1) {
    DWORD oldP = 0;
    auto* p = reinterpret_cast<unsigned char*>(a);
    if (!VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &oldP)) return false;
    if (fill >= 0) {
        for (std::size_t i = 0; i < n; ++i) p[i] = static_cast<unsigned char>(fill);
    } else {
        auto* s = static_cast<const unsigned char*>(d);
        for (std::size_t i = 0; i < n; ++i) p[i] = s[i];
    }
    FlushInstructionCache(GetCurrentProcess(), p, n);
    DWORD r2 = 0;
    VirtualProtect(p, n, oldP, &r2);
    return true;
}

template <typename T>
bool W(std::uintptr_t a, const T& v) { return Patch(a, &v, sizeof(v)); }
bool Nop(std::uintptr_t a, std::size_t n) { return Patch(a, nullptr, n, 0x90); }

bool Br(std::uintptr_t a, std::uintptr_t t, std::uint8_t op) {
    const std::int64_t rel = static_cast<std::int64_t>(t) - static_cast<std::int64_t>(a + sizeof(RelBranch));
    if (rel < std::numeric_limits<std::int32_t>::min() || rel > std::numeric_limits<std::int32_t>::max()) return false;
    const RelBranch pb{ op, static_cast<std::int32_t>(rel) };
    return Patch(a, &pb, sizeof(pb));
}

template <typename T>
bool WPtr(std::uintptr_t a, T* p) { return W(a, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p))); }

DWORD GameState() { return *reinterpret_cast<volatile const DWORD*>(kGtaLoadState); }

bool Tok(const char* cmd, const char* tok) {
    if (!cmd || !tok || !*tok) return false;
    std::size_t n = 0;
    while (tok[n]) ++n;
    auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? char(c + 32) : c; };
    auto bound = [](char c) {
        return !c || c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '"' || c == '\'';
    };
    for (const char* p = cmd; *p; ++p) {
        if (p != cmd && !bound(p[-1])) continue;
        std::size_t i = 0;
        while (i < n && p[i] && lower(p[i]) == tok[i]) ++i;
        if (i == n && bound(p[n])) return true;
    }
    return false;
}

bool IsSamp() {
    return GetModuleHandleA("samp.dll") || Tok(GetCommandLineA(), "-c") || Tok(GetCommandLineA(), "-h")
        || Tok(GetCommandLineA(), "-p");
}

bool SlotPath(char* out, std::size_t cap) {
    const char* base = reinterpret_cast<GetUserFilesDirFn>(kGetUserFiles)();
    if (!base || !*base) return false;
    std::size_t i = 0;
    while (base[i] && i + 1 < cap) out[i] = base[i], ++i;
    if (!i || i + 1 >= cap) return false;
    if (out[i - 1] != '\\' && out[i - 1] != '/') out[i++] = '\\';
    for (std::size_t j = 0; kSlotFile[j] && i + 1 < cap; ++j) out[i++] = kSlotFile[j];
    out[i] = '\0';
    return true;
}

int ReadSlot() {
    char path[MAX_PATH];
    if (!SlotPath(path, MAX_PATH)) return kNoSlot;
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return kNoSlot;
    unsigned char s = 0xFF;
    DWORD r = 0;
    const BOOL ok = ReadFile(f, &s, 1, &r, nullptr);
    CloseHandle(f);
    return (ok && r == 1 && s < kMaxSlots) ? static_cast<int>(s) : kNoSlot;
}

bool WriteSlot(int slot) {
    if (slot < 0 || slot >= kMaxSlots) return false;
    char path[MAX_PATH];
    if (!SlotPath(path, MAX_PATH)) return false;
    HANDLE f = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    unsigned char v = static_cast<unsigned char>(slot);
    DWORD w = 0;
    const BOOL ok = WriteFile(f, &v, 1, &w, nullptr);
    CloseHandle(f);
    return ok && w == 1;
}

bool SlotOk(int s) {
    return s >= 0 && s < kMaxSlots && reinterpret_cast<bool(__cdecl*)(int)>(kCheckSlot)(s);
}

void LoadSlot(int slot) {
    auto* m = reinterpret_cast<volatile unsigned char*>(kMenuMgr);
    m[M_DEACT] = 0;
    m[M_SAVE] = static_cast<unsigned char>(slot);
    *reinterpret_cast<volatile unsigned char*>(kMissionPack) = 0;
    m[M_ITEM] = 13;
    m[M_START] = 1;
    *reinterpret_cast<volatile unsigned char*>(kUserPause) = 0;
}

DWORD __stdcall RsCamNop() { return 0; }

void __cdecl FrontendIdleSP() {
    static int ticks = 0;
    static bool tried = false, autoLd = false;
    static int lastW = kNoSlot;
    auto* m = reinterpret_cast<volatile unsigned char*>(kMenuMgr);

    ++ticks;
    const DWORD gs = GameState();
    const bool hide = gs < 9 && ((ticks <= 2) || (autoLd && ticks <= kHideMenuTicks));

    if (hide) (void)Br(kCallRsCameraBegin, reinterpret_cast<std::uintptr_t>(&RsCamNop), 0xE8);

    if (!tried && ticks >= 2) {
        tried = true;
        const int st = ReadSlot();
        if (SlotOk(st)) {
            LoadSlot(st);
            lastW = st;
            autoLd = true;
        }
    }

    const int sel = static_cast<int>(m[M_SAVE]);
    if (m[M_ITEM] == 13 && m[M_START] == 1 && sel >= 0 && sel < kMaxSlots && sel != lastW && WriteSlot(sel))
        lastW = sel;

    reinterpret_cast<void(__cdecl*)()>(kFrontendIdle)();

    if (hide) (void)Br(kCallRsCameraBegin, kRsCameraBegin, 0xE8);
}

void PortableOnce() {
    if (GetModuleHandleA("portablegta.asi") || GetModuleHandleA("PortableGTA.asi"))
        (void)reinterpret_cast<GetUserFilesDirFn>(kGetUserFiles)();
}

void __cdecl IncSplash() {
    auto* cur = reinterpret_cast<volatile int*>(kSplashIdx);
    int n = *cur;
    g_prevSplash = n + 1;
    if (++n >= 7) n = 1;
    *cur = n;
}

void __cdecl SimCopyright() {
    *reinterpret_cast<volatile int*>(kSplashIdx) = 0;
    *reinterpret_cast<volatile float*>(kTimeSplash) -= 1000.0f;
    *reinterpret_cast<volatile unsigned char*>(kFirstLoadSplash) = 1;
}

bool PatchSp() {
    static constexpr std::uint8_t kForceGs[] = { 0xC7, 0x05, 0xC0, 0xD4, 0xC8, 0x00, 0x05, 0x00, 0x00, 0x00 };
    static constexpr std::uint8_t kRetLoadScr[] = { 0xC3, 0x90, 0x90, 0x90, 0x90 };
    bool ok = true;
    ok &= Nop(kNopGameStateReset, 6);
    ok &= W(kGtaLoadState, kTargetGameState);
    ok &= Nop(kNopLoadAudio, 5);
    ok &= Nop(kPatchGameStateWrite, 6);
    ok &= Patch(kPatchGameStateWrite, kForceGs, sizeof(kForceGs));
    ok &= Nop(kNopPreCopyright, 5);
    ok &= Nop(kNopCopyrightJmp, 5);
    ok &= Br(kCallCopyright, reinterpret_cast<std::uintptr_t>(&SimCopyright), 0xE8);
    ok &= Br(kJmpSkipFade, kSkipFadeTarget, 0xE9);
    ok &= Br(kCallIncSplash, reinterpret_cast<std::uintptr_t>(&IncSplash), 0xE8);
    ok &= Nop(kCallIncSplash + 5, 1);
    for (std::uintptr_t a : kPrevSplashOps) ok &= WPtr(a, &g_prevSplash);
    ok &= WPtr(kOpLoadscreenTime, &g_loadscreenTime);
    ok &= Nop(kNopLoadBar, 5);
    ok &= Patch(kPatchLoadScreen, kRetLoadScr, sizeof(kRetLoadScr));
    return ok;
}

bool PatchSamp() {
    bool ok = PatchSp();
    ok &= W<std::uint8_t>(kSampForeground, 1);
    ok &= W<std::uint8_t>(kMenuFocusLoss, 0);
    ok &= W<std::uint8_t>(kUnpauseWhenPaused, 0);
    ok &= Nop(kNopIdleWhenMin, 2);
    return ok;
}

void Init() {
    if (GameState() >= 9) return;
    const bool samp = IsSamp();
    bool ok = samp ? PatchSamp() : PatchSp();
    if (!samp && ok) ok &= Br(kCallFrontendIdle, reinterpret_cast<std::uintptr_t>(&FrontendIdleSP), 0xE8);
    if (ok) PortableOnce();
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE m, DWORD why, LPVOID) {
    if (why == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(m), Init();
    return TRUE;
}
