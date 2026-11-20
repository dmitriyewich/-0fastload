#pragma once

#include <windows.h>

#include <cstddef>

namespace fastload_launch {
namespace detail {

// A DNS name can take 253 characters, plus the flag and terminator.
constexpr std::size_t kArgumentCapacity = 256;

inline bool IsSpace(wchar_t ch) { return ch == L' ' || ch == L'\t'; }
inline bool IsFlagPrefix(wchar_t ch) { return ch == L'-' || ch == L'/'; }
inline wchar_t LowerAscii(wchar_t ch) {
    return ch >= L'A' && ch <= L'Z' ? static_cast<wchar_t>(ch + (L'a' - L'A')) : ch;
}

inline const wchar_t* SkipProgramName(const wchar_t* cursor) {
    bool quoted = false;
    while (*cursor && (quoted || !IsSpace(*cursor))) {
        if (*cursor++ == L'"') quoted = !quoted;
    }
    return cursor;
}

inline const wchar_t* ReadArgument(const wchar_t* cursor, wchar_t (&argument)[kArgumentCapacity]) {
    std::size_t length = 0;
    auto append = [&](wchar_t ch) {
        if (length < kArgumentCapacity - 1) argument[length] = ch;
        if (length < kArgumentCapacity) ++length;
    };

    bool quoted = false;
    while (*cursor && (quoted || !IsSpace(*cursor))) {
        std::size_t slashes = 0;
        while (*cursor == L'\\') {
            ++slashes;
            ++cursor;
        }
        if (*cursor == L'"') {
            for (std::size_t i = 0; i < slashes / 2; ++i) append(L'\\');
            if (slashes % 2) {
                append(L'"');
            } else if (quoted && cursor[1] == L'"') {
                append(L'"');
                ++cursor;
            } else {
                quoted = !quoted;
            }
            ++cursor;
            continue;
        }
        for (std::size_t i = 0; i < slashes; ++i) append(L'\\');
        if (!*cursor || (!quoted && IsSpace(*cursor))) break;
        append(*cursor++);
    }

    argument[length < kArgumentCapacity ? length : 0] = L'\0';
    return cursor;
}

inline bool IsPort(const wchar_t* value) {
    unsigned port = 0;
    for (; *value; ++value) {
        if (*value < L'0' || *value > L'9') return false;
        port = port * 10 + static_cast<unsigned>(*value - L'0');
        if (port > 65535) return false;
    }
    return port != 0;
}

inline bool IsHost(const wchar_t* value, bool attached) {
    constexpr wchar_t kLocalhost[] = L"localhost";
    if (!*value || *value == L'-') return false;

    bool hasNamePart = false;
    bool dotted = false;
    unsigned colons = 0;
    std::size_t length = 0;
    bool localhost = true;
    for (const wchar_t* cursor = value; *cursor; ++cursor, ++length) {
        const wchar_t ch = LowerAscii(*cursor);
        if (localhost && (length >= sizeof(kLocalhost) / sizeof(*kLocalhost) - 1 || ch != kLocalhost[length]))
            localhost = false;
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'0' && ch <= L'9') || ch >= L'\x0080') {
            hasNamePart = true;
        } else if (ch == L'.') {
            dotted = true;
        } else if (ch == L':') {
            ++colons;
        } else if (ch != L'-' && ch != L'_' && !(ch == L'[' && cursor == value) &&
                   !(ch == L']' && !cursor[1])) {
            return false;
        }
    }
    if (!hasNamePart && colons < 2) return false;
    // Bare attached host names are ambiguous with switches like -help.
    return !attached || dotted || colons >= 2 || (localhost && length == sizeof(kLocalhost) / sizeof(*kLocalhost) - 1);
}

}  // namespace detail

inline bool HasSampArguments(const wchar_t* commandLine) {
    if (!commandLine) return false;

    const wchar_t* cursor = detail::SkipProgramName(commandLine);
    wchar_t pendingValue = L'\0';
    while (*cursor) {
        while (detail::IsSpace(*cursor)) ++cursor;
        wchar_t argument[detail::kArgumentCapacity];
        cursor = detail::ReadArgument(cursor, argument);

        if ((pendingValue == L'h' && detail::IsHost(argument, false)) ||
            (pendingValue == L'p' && detail::IsPort(argument))) return true;
        pendingValue = L'\0';

        if (!detail::IsFlagPrefix(argument[0]) || !argument[1]) continue;
        const wchar_t flag = detail::LowerAscii(argument[1]);
        const wchar_t* value = argument + 2;
        if (!*value) {
            if (flag == L'c' || flag == L'd') return true;
            if (flag == L'h' || flag == L'p') pendingValue = flag;
        } else if ((flag == L'h' && detail::IsHost(value, true)) ||
                   (flag == L'p' && detail::IsPort(value))) {
            return true;
        }
    }
    return false;
}

inline bool SampModuleLoaded() { return GetModuleHandleW(L"samp.dll") != nullptr; }
inline bool LooksLikeSamp() { return SampModuleLoaded() || HasSampArguments(GetCommandLineW()); }

}  // namespace fastload_launch
