#pragma once

namespace fastload_patch {

constexpr std::uintptr_t kImageBase = 0x00400000;
constexpr std::size_t kPageSize = 0x1000;
constexpr std::size_t kMaxPatches = 6;

struct Patch {
    std::uintptr_t address;
    std::size_t size;
    unsigned char original[10];
    unsigned char replacement[10];
};

template <std::size_t Size>
constexpr Patch Bytes(std::uintptr_t address, const unsigned char (&original)[Size],
                      const unsigned char (&replacement)[Size]) {
    static_assert(Size <= sizeof(Patch::original));
    Patch patch{address, Size, {}, {}};
    for (std::size_t i = 0; i < Size; ++i) {
        patch.original[i] = original[i];
        patch.replacement[i] = replacement[i];
    }
    return patch;
}

enum class Access { Read, Write, Code };

inline bool ImageMemory(std::uintptr_t address, std::size_t size, std::uintptr_t base, Access access) {
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &region, sizeof(region))
        || region.State != MEM_COMMIT || region.Type != MEM_IMAGE
        || reinterpret_cast<std::uintptr_t>(region.AllocationBase) != base
        || (region.Protect & PAGE_GUARD)) return false;

    const DWORD protection = region.Protect & 0xFF;
    const bool code = protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE
                   || protection == PAGE_EXECUTE_WRITECOPY;
    const bool data = protection == PAGE_READONLY || protection == PAGE_READWRITE
                   || protection == PAGE_WRITECOPY;
    const bool write = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY
                    || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    const bool allowed = access == Access::Code ? code : access == Access::Write ? write : (code || data);
    const auto offset = address - reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    return allowed && offset < region.RegionSize && size <= region.RegionSize - offset;
}

inline bool Equal(std::uintptr_t address, const unsigned char* bytes, std::size_t size) {
    const auto* actual = reinterpret_cast<const volatile unsigned char*>(address);
    for (std::size_t i = 0; i < size; ++i) {
        if (actual[i] != bytes[i]) return false;
    }
    return true;
}

inline bool Matches(const Patch& patch, bool reverse = false) {
    return ImageMemory(patch.address, patch.size, kImageBase, Access::Code)
        && Equal(patch.address, reverse ? patch.replacement : patch.original, patch.size);
}

#pragma pack(push, 1)
struct RelativeCall {
    unsigned char opcode;
    std::uint32_t displacement;
};
#pragma pack(pop)

static_assert(sizeof(RelativeCall) == 5);

inline std::uintptr_t Target(std::uintptr_t site, RelativeCall instruction) {
    return site + sizeof(instruction) + instruction.displacement;
}

inline bool ReadBranch(std::uintptr_t site, unsigned char opcode, RelativeCall& branch) {
    if (!ImageMemory(site, sizeof(branch), kImageBase, Access::Code)) return false;
    memcpy(&branch, reinterpret_cast<const void*>(site), sizeof(branch));
    return branch.opcode == opcode;
}

inline bool BranchTo(std::uintptr_t site, unsigned char opcode, std::uintptr_t target) {
    RelativeCall branch{};
    return ReadBranch(site, opcode, branch) && Target(site, branch) == target;
}

template <typename Function>
bool Call(Patch& patch, std::uintptr_t site, std::uintptr_t vanilla, std::uintptr_t hook,
          Function& original, HMODULE compatibleModule = nullptr) {
    RelativeCall prior{};
    if (!ReadBranch(site, 0xE8, prior)) return false;
    const auto target = Target(site, prior);
    const auto module = target == vanilla ? kImageBase : reinterpret_cast<std::uintptr_t>(compatibleModule);
    if (!module || !ImageMemory(target, 1, module, Access::Code)) return false;
    const RelativeCall replacement{0xE8, static_cast<std::uint32_t>(hook - site - sizeof(RelativeCall))};
    patch.address = site;
    patch.size = sizeof(prior);
    memcpy(patch.original, &prior, sizeof(prior));
    memcpy(patch.replacement, &replacement, sizeof(replacement));
    original = reinterpret_cast<Function>(target);
    return true;
}

struct Page {
    std::uintptr_t address;
    DWORD protection;
    bool open;
};

inline bool Open(Page& page, bool saveProtection) {
    if (page.open) return true;
    DWORD previous = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(page.address), kPageSize, PAGE_EXECUTE_READWRITE, &previous))
        return false;
    if (saveProtection) page.protection = previous;
    page.open = true;
    return true;
}

inline bool Close(Page* pages, std::size_t count) {
    bool restored = true;
    for (std::size_t i = 0; i < count; ++i) {
        auto& page = pages[i];
        if (!page.open) continue;
        DWORD previous = 0;
        if (VirtualProtect(reinterpret_cast<void*>(page.address), kPageSize, page.protection, &previous))
            page.open = false;
        else
            restored = false;
    }
    return restored;
}

inline bool Flush(const Page* pages, std::size_t count) {
    bool flushed = true;
    const HANDLE process = GetCurrentProcess();
    for (std::size_t i = 0; i < count; ++i) {
        if (!FlushInstructionCache(process, reinterpret_cast<const void*>(pages[i].address), kPageSize))
            flushed = false;
    }
    return flushed;
}

// All callers apply at startup, or restore from the owning WinMain thread.
inline bool Apply(const Patch* patches, std::size_t count, bool reverse = false) {
    if (!count || count > kMaxPatches) return false;
    Page pages[kMaxPatches]{};
    std::size_t pageForPatch[kMaxPatches]{};
    std::size_t pageCount = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& patch = patches[i];
        const auto page = patch.address & ~(kPageSize - 1);
        if (!patch.size || patch.size > sizeof(patch.original)
            || patch.address - page + patch.size > kPageSize || !Matches(patch, reverse)) return false;
        std::size_t index = 0;
        while (index < pageCount && pages[index].address != page) ++index;
        if (index == pageCount) pages[pageCount++].address = page;
        pageForPatch[i] = index;
    }

    for (std::size_t i = 0; i < pageCount; ++i) {
        if (!Open(pages[i], true)) {
            if (!Close(pages, pageCount)) (void)Close(pages, pageCount);
            return false;
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto& patch = patches[i];
        if (!Equal(patch.address, reverse ? patch.replacement : patch.original, patch.size)) {
            if (!Close(pages, pageCount)) (void)Close(pages, pageCount);
            return false;
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto& patch = patches[i];
        memcpy(reinterpret_cast<void*>(patch.address), reverse ? patch.original : patch.replacement, patch.size);
    }

    if (Flush(pages, pageCount) && Close(pages, pageCount)) return true;

    for (std::size_t i = 0; i < pageCount; ++i) (void)Open(pages[i], false);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& patch = patches[i];
        const auto* written = reverse ? patch.original : patch.replacement;
        if (pages[pageForPatch[i]].open && Equal(patch.address, written, patch.size))
            memcpy(reinterpret_cast<void*>(patch.address), reverse ? patch.replacement : patch.original, patch.size);
    }
    if (!Flush(pages, pageCount)) (void)Flush(pages, pageCount);
    if (!Close(pages, pageCount)) (void)Close(pages, pageCount);
    return false;
}

}  // namespace fastload_patch
