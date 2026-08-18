#pragma once

#include <array>
#include "Common.hpp"
#include "SearchLinear.hpp"

// TODO: implement SVE functions if and when I get access to SVE-capable hardware.

namespace blam::simd {
    template <std::array SignificantChars>
    BLAM_TARGET_SVE char const* skipNonSignificantSVE(char const* ptr, char const* end) noexcept {
        return skipNonSignificantLinear<SignificantChars>(ptr, end);
    }

    BLAM_TARGET_SVE inline bool hasNonWhitespaceSVE(char const* ptr, char const* end) noexcept {
        return hasNonWhitespaceLinear(ptr, end);
    }

    BLAM_TARGET_SVE inline char const* findAnyOfThreeSVE(char const* ptr, char const* end, char a, char b, char c) noexcept {
        return findAnyOfThreeLinear(ptr, end, a, b, c);
    }

    template <std::array SignificantChars>
    BLAM_TARGET_SVE SkipClassifiedResult skipNonSignificantClassifiedSVE(char const* ptr, char const* end) noexcept {
        return skipNonSignificantClassifiedLinear<SignificantChars>(ptr, end);
    }
}