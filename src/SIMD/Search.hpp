#pragma once

#include <algorithm>
#include <array>

#include "Common.hpp"
#include "SearchLinear.hpp"

#ifdef BLAM_PLATFORM_X86_64
#include "SearchAVX2.hpp"
#include "SearchSSE2.hpp"
#elif defined(BLAM_PLATFORM_ARM64)
#include "SearchNEON.hpp"
#include "SearchSVE.hpp"
#endif

namespace blam::simd {
    template <std::array SignificantChars>
    char const* skipNonSignificant(char const* ptr, char const* end) noexcept {
    #ifdef BLAM_HAS_AVX2
        return skipNonSignificantAVX2<SignificantChars>(ptr, end);
    #elif defined(BLAM_HAS_SVE)
        return skipNonSignificantSVE<SignificantChars>(ptr, end);
    #else
        using Fn = char const*(*)(char const*, char const*);
        static Fn const func = [] -> Fn {
            auto features = detectCPUFeatures();
        #ifdef BLAM_PLATFORM_X86_64
            if (features.avx2) return &skipNonSignificantAVX2<SignificantChars>;
            if (features.sse2) return &skipNonSignificantSSE2<SignificantChars>;
        #elif defined(BLAM_PLATFORM_ARM64)
            if (features.sve) return &skipNonSignificantSVE<SignificantChars>;
            if (features.neon) return &skipNonSignificantNEON<SignificantChars>;
        #endif
            return &skipNonSignificantLinear<SignificantChars>;
        }();
        return func(ptr, end);
    #endif
    }

    inline char const* skipToNewline(char const* ptr, char const* end) noexcept {
        return skipNonSignificant<std::array<char, 0>{}>(ptr, end);
    }

    inline bool hasNonWhitespace(char const* ptr, char const* end) noexcept {
    #ifdef BLAM_HAS_AVX2
        return hasNonWhitespaceAVX2(ptr, end);
    #elif defined(BLAM_HAS_SVE)
        return hasNonWhitespaceSVE(ptr, end);
    #else
        using Fn = bool(*)(char const*, char const*);
        static Fn const func = [] -> Fn {
            auto features = detectCPUFeatures();
        #ifdef BLAM_PLATFORM_X86_64
            if (features.avx2) return &hasNonWhitespaceAVX2;
            if (features.sse2) return &hasNonWhitespaceSSE2;
        #elif defined(BLAM_PLATFORM_ARM64)
            if (features.sve) return &hasNonWhitespaceSVE;
            if (features.neon) return &hasNonWhitespaceNEON;
        #endif
            return &hasNonWhitespaceLinear;
        }();
        return func(ptr, end);
    #endif
    }

    inline char const* findAnyOfThree(char const* ptr, char const* end, char a, char b, char c) noexcept {
    #ifdef BLAM_HAS_AVX2
        return findAnyOfThreeAVX2(ptr, end, a, b, c);
    #elif defined(BLAM_HAS_SVE)
        return findAnyOfThreeSVE(ptr, end, a, b, c);
    #else
        using Fn = char const*(*)(char const*, char const*, char, char, char);
        static Fn const func = [] -> Fn {
            auto features = detectCPUFeatures();
        #ifdef BLAM_PLATFORM_X86_64
            if (features.avx2) return &findAnyOfThreeAVX2;
            if (features.sse2) return &findAnyOfThreeSSE2;
        #elif defined(BLAM_PLATFORM_ARM64)
            if (features.sve) return &findAnyOfThreeSVE;
            if (features.neon) return &findAnyOfThreeNEON;
        #endif
            return &findAnyOfThreeLinear;
        }();
        return func(ptr, end, a, b, c);
    #endif
    }

    template <std::array SignificantChars>
    SkipClassifiedResult skipNonSignificantClassified(char const* ptr, char const* end) noexcept {
    #ifdef BLAM_HAS_AVX2
        return skipNonSignificantClassifiedAVX2<SignificantChars>(ptr, end);
    #elif defined(BLAM_HAS_SVE)
        return skipNonSignificantClassifiedSVE<SignificantChars>(ptr, end);
    #else
        using Fn = SkipClassifiedResult(*)(char const*, char const*);
        static Fn const func = [] -> Fn {
            auto features = detectCPUFeatures();
        #ifdef BLAM_PLATFORM_X86_64
            if (features.avx2) return &skipNonSignificantClassifiedAVX2<SignificantChars>;
            if (features.sse2) return &skipNonSignificantClassifiedSSE2<SignificantChars>;
        #elif defined(BLAM_PLATFORM_ARM64)
            if (features.sve) return &skipNonSignificantClassifiedSVE<SignificantChars>;
            if (features.neon) return &skipNonSignificantClassifiedNEON<SignificantChars>;
        #endif
            return &skipNonSignificantClassifiedLinear<SignificantChars>;
        }();
        return func(ptr, end);
    #endif
    }
}
