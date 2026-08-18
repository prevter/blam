#pragma once

#include <array>

namespace blam::simd {
    template <std::array SignificantChars>
    constexpr char const* skipNonSignificantLinear(char const* ptr, char const* end) noexcept {
        while (ptr < end) {
            if (*ptr == '\n') return ptr;
            for (char c : SignificantChars) {
                if (*ptr == c) return ptr;
            }
            ++ptr;
        }
        return ptr;
    }

    constexpr bool hasNonWhitespaceLinear(char const* start, char const* end) noexcept {
        while (start < end) {
            if (*start != ' ' && *start != '\t' && *start != '\r') return true;
            ++start;
        }
        return false;
    }

    constexpr char const* findAnyOfThreeLinear(char const* ptr, char const* end, char a, char b, char c) noexcept {
        for (; ptr < end; ++ptr) {
            char ch = *ptr;
            if (ch == a || ch == b || ch == c) return ptr;
        }
        return end;
    }

    struct SkipClassifiedResult {
        char const* ptr;
        bool sawNonWhitespace;
    };

    template <std::array SignificantChars>
    constexpr SkipClassifiedResult skipNonSignificantClassifiedLinear(char const* ptr, char const* end) noexcept {
        bool sawNonWs = false;
        while (ptr < end) {
            char c = *ptr;
            if (c == '\n') return {ptr, sawNonWs};
            for (char sc : SignificantChars) {
                if (c == sc) return {ptr, sawNonWs};
            }
            if (c != ' ' && c != '\t' && c != '\r') sawNonWs = true;
            ++ptr;
        }
        return {ptr, sawNonWs};
    }
}