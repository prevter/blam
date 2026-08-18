#pragma once

#include <algorithm>
#include <array>
#include "Common.hpp"

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
            if (ch == a | ch == b | ch == c) return ptr;
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

#ifdef BLAM_PLATFORM_X86_64
    template <std::array SignificantChars>
    BLAM_TARGET("avx2") std::array<__m256i, SignificantChars.size() + 1> buildVCharsAVX2() noexcept {
        constexpr size_t N = SignificantChars.size();
        std::array<__m256i, N + 1> chars;
        chars[0] = _mm256_set1_epi8('\n');
        for (size_t i = 1; i <= N; ++i) {
            chars[i] = _mm256_set1_epi8(SignificantChars[i - 1]);
        }
        return chars;
    }

    template <std::array SignificantChars>
    BLAM_TARGET("sse2") std::array<__m128i, SignificantChars.size() + 1> buildVCharsSSE2() noexcept {
        constexpr size_t N = SignificantChars.size();
        std::array<__m128i, N + 1> chars;
        chars[0] = _mm_set1_epi8('\n');
        for (size_t i = 1; i <= N; ++i) {
            chars[i] = _mm_set1_epi8(SignificantChars[i - 1]);
        }
        return chars;
    }

    template <std::array SignificantChars>
    BLAM_TARGET("avx2") char const* skipNonSignificantAVX2(char const* ptr, char const* end) noexcept {
        constexpr size_t N = SignificantChars.size();

        static auto const vchars = buildVCharsAVX2<SignificantChars>();

        while (ptr + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(ptr));
            __m256i cmp = _mm256_cmpeq_epi8(chunk, vchars[0]);
            for (size_t i = 1; i <= N; ++i) {
                cmp = _mm256_or_si256(cmp, _mm256_cmpeq_epi8(chunk, vchars[i]));
            }

            int mask = _mm256_movemask_epi8(cmp);
            if (mask != 0) {
                return ptr + __builtin_ctz(mask);
            }

            ptr += 32;
        }

        return skipNonSignificantLinear<SignificantChars>(ptr, end);
    }

    template <std::array SignificantChars>
    BLAM_TARGET("sse2") char const* skipNonSignificantSSE2(char const* ptr, char const* end) noexcept {
        constexpr size_t N = SignificantChars.size();

        static auto const vchars = buildVCharsSSE2<SignificantChars>();

        while (ptr + 16 <= end) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<__m128i const*>(ptr));
            __m128i cmp = _mm_cmpeq_epi8(chunk, vchars[0]);
            for (size_t i = 1; i <= N; ++i) {
                cmp = _mm_or_si128(cmp, _mm_cmpeq_epi8(chunk, vchars[i]));
            }

            int mask = _mm_movemask_epi8(cmp);
            if (mask != 0) {
                return ptr + __builtin_ctz(mask);
            }

            ptr += 16;
        }

        return skipNonSignificantLinear<SignificantChars>(ptr, end);
    }

    BLAM_TARGET("avx2") inline bool hasNonWhitespaceAVX2(char const* ptr, char const* end) noexcept {
        __m256i space = _mm256_set1_epi8(' ');
        __m256i tab = _mm256_set1_epi8('\t');
        __m256i cr = _mm256_set1_epi8('\r');

        while (ptr + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(ptr));
            __m256i cmp = _mm256_or_si256(
                _mm256_or_si256(_mm256_cmpeq_epi8(chunk, space), _mm256_cmpeq_epi8(chunk, tab)),
                _mm256_cmpeq_epi8(chunk, cr)
            );

            if (static_cast<uint32_t>(_mm256_movemask_epi8(cmp)) != 0xFFFFFFFFu) {
                return true;
            }

            ptr += 32;
        }

        return hasNonWhitespaceLinear(ptr, end);
    }

    BLAM_TARGET("sse2") inline bool hasNonWhitespaceSSE2(char const* ptr, char const* end) noexcept {
        __m128i space = _mm_set1_epi8(' ');
        __m128i tab = _mm_set1_epi8('\t');
        __m128i cr = _mm_set1_epi8('\r');

        while (ptr + 16 <= end) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<__m128i const*>(ptr));
            __m128i cmp = _mm_or_si128(
                _mm_or_si128(_mm_cmpeq_epi8(chunk, space), _mm_cmpeq_epi8(chunk, tab)),
                _mm_cmpeq_epi8(chunk, cr)
            );

            if (_mm_movemask_epi8(cmp) != 0xFFFF) {
                return true;
            }

            ptr += 16;
        }

        return hasNonWhitespaceLinear(ptr, end);
    }

    BLAM_TARGET("avx2") inline char const* findAnyOfThreeAVX2(char const* ptr, char const* end, char a, char b, char c) noexcept {
        __m256i va = _mm256_set1_epi8(a);
        __m256i vb = _mm256_set1_epi8(b);
        __m256i vc = _mm256_set1_epi8(c);

        while (ptr + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(ptr));
            __m256i cmp = _mm256_or_si256(
                _mm256_or_si256(
                    _mm256_cmpeq_epi8(chunk, va),
                    _mm256_cmpeq_epi8(chunk, vb)
                ),
                _mm256_cmpeq_epi8(chunk, vc)
            );

            int mask = _mm256_movemask_epi8(cmp);
            if (mask != 0) return ptr + __builtin_ctz(mask);
            ptr += 32;
        }

        return findAnyOfThreeLinear(ptr, end, a, b, c);
    }

    BLAM_TARGET("sse2") inline char const* findAnyOfThreeSSE2(char const* ptr, char const* end, char a, char b, char c) noexcept {
        __m128i va = _mm_set1_epi8(a);
        __m128i vb = _mm_set1_epi8(b);
        __m128i vc = _mm_set1_epi8(c);

        while (ptr + 16 <= end) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<__m128i const*>(ptr));
            __m128i cmp = _mm_or_si128(
                _mm_or_si128(
                    _mm_cmpeq_epi8(chunk, va),
                    _mm_cmpeq_epi8(chunk, vb)
                ),
                _mm_cmpeq_epi8(chunk, vc)
            );

            int mask = _mm_movemask_epi8(cmp);
            if (mask != 0) return ptr + __builtin_ctz(mask);
            ptr += 16;
        }

        return findAnyOfThreeLinear(ptr, end, a, b, c);
    }

    template <std::array SignificantChars>
    BLAM_TARGET("avx2") SkipClassifiedResult skipNonSignificantClassifiedAVX2(char const* ptr, char const* end) noexcept {
        constexpr size_t N = SignificantChars.size();

        static auto const vchars = buildVCharsAVX2<SignificantChars>();

        __m256i vsp = _mm256_set1_epi8(' ');
        __m256i vtab = _mm256_set1_epi8('\t');
        __m256i vcr = _mm256_set1_epi8('\r');

        bool sawNonWs = false;

        while (ptr + 32 <= end) {
            __m256i chunk = _mm256_loadu_si256(reinterpret_cast<__m256i const*>(ptr));

            __m256i sigCmp = _mm256_cmpeq_epi8(chunk, vchars[0]);
            for (size_t i = 1; i <= N; ++i)
                sigCmp = _mm256_or_si256(sigCmp, _mm256_cmpeq_epi8(chunk, vchars[i]));
            uint32_t sigMask = static_cast<uint32_t>(_mm256_movemask_epi8(sigCmp));

            __m256i wsCmp = _mm256_or_si256(
                _mm256_or_si256(
                    _mm256_cmpeq_epi8(chunk, vsp),
                    _mm256_cmpeq_epi8(chunk, vtab)
                ),
                _mm256_cmpeq_epi8(chunk, vcr)
            );
            uint32_t wsMask = static_cast<uint32_t>(_mm256_movemask_epi8(wsCmp));

            if (sigMask != 0) {
                uint32_t pos = static_cast<uint32_t>(__builtin_ctz(sigMask));
                if (pos > 0) {
                    uint32_t prefix = (1u << pos) - 1;
                    if ((wsMask & prefix) != prefix) sawNonWs = true;
                }
                return {ptr + pos, sawNonWs};
            }

            if (wsMask != 0xFFFFFFFFu) sawNonWs = true;
            ptr += 32;
        }

        auto [fp, fNw] = skipNonSignificantClassifiedLinear<SignificantChars>(ptr, end);
        return {fp, static_cast<bool>(sawNonWs | fNw)};
    }

    template <std::array SignificantChars>
    BLAM_TARGET("sse2") SkipClassifiedResult skipNonSignificantClassifiedSSE2(char const* ptr, char const* end) noexcept {
        constexpr size_t N = SignificantChars.size();

        static auto const vchars = buildVCharsSSE2<SignificantChars>();

        __m128i vsp = _mm_set1_epi8(' ');
        __m128i vtab = _mm_set1_epi8('\t');
        __m128i vcr = _mm_set1_epi8('\r');

        bool sawNonWs = false;

        while (ptr + 16 <= end) {
            __m128i chunk = _mm_loadu_si128(reinterpret_cast<__m128i const*>(ptr));

            __m128i sigCmp = _mm_cmpeq_epi8(chunk, vchars[0]);
            for (size_t i = 1; i <= N; ++i)
                sigCmp = _mm_or_si128(sigCmp, _mm_cmpeq_epi8(chunk, vchars[i]));
            uint32_t sigMask = static_cast<uint32_t>(_mm_movemask_epi8(sigCmp));

            __m128i wsCmp = _mm_or_si128(
                _mm_or_si128(
                    _mm_cmpeq_epi8(chunk, vsp),
                    _mm_cmpeq_epi8(chunk, vtab)
                ),
                _mm_cmpeq_epi8(chunk, vcr)
            );
            uint32_t wsMask = static_cast<uint32_t>(_mm_movemask_epi8(wsCmp)) & 0xFFFFu;

            if (sigMask != 0) {
                uint32_t pos = static_cast<uint32_t>(__builtin_ctz(sigMask));
                if (pos > 0) {
                    uint32_t prefix = (1u << pos) - 1;
                    if ((wsMask & prefix) != prefix) sawNonWs = true;
                }
                return {ptr + pos, sawNonWs};
            }

            if (wsMask != 0xFFFFu) sawNonWs = true;
            ptr += 16;
        }

        auto [fp, fNw] = skipNonSignificantClassifiedLinear<SignificantChars>(ptr, end);
        return {fp, static_cast<bool>(sawNonWs | fNw)};
    }
#elif defined(BLAM_PLATFORM_ARM64)
    template <std::array SignificantChars>
    char const* skipNonSignificantSVE(char const* ptr, char const* end) noexcept {
        return skipNonSignificantLinear<SignificantChars>(ptr, end);
    }

    template <std::array SignificantChars>
    char const* skipNonSignificantNEON(char const* ptr, char const* end) noexcept {
        return skipNonSignificantLinear<SignificantChars>(ptr, end);
    }

    inline bool hasNonWhitespaceSVE(char const* ptr, char const* end) noexcept {
        return hasNonWhitespaceLinear(ptr, end);
    }

    inline bool hasNonWhitespaceNEON(char const* ptr, char const* end) noexcept {
        return hasNonWhitespaceLinear(ptr, end);
    }

    inline char const* findAnyOfThreeSVE(char const* ptr, char const* end, char a, char b, char c) noexcept {
        return findAnyOfThreeLinear(ptr, end, a, b, c);
    }

    inline char const* findAnyOfThreeNEON(char const* ptr, char const* end, char a, char b, char c) noexcept {
        return findAnyOfThreeLinear(ptr, end, a, b, c);
    }

    template <std::array SignificantChars>
    SkipClassifiedResult skipNonSignificantClassifiedSVE(char const* ptr, char const* end) noexcept {
        return skipNonSignificantClassifiedLinear<SignificantChars>(ptr, end);
    }

    template <std::array SignificantChars>
    SkipClassifiedResult skipNonSignificantClassifiedNEON(char const* ptr, char const* end) noexcept {
        return skipNonSignificantClassifiedLinear<SignificantChars>(ptr, end);
    }
#endif

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
