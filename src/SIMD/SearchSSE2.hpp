#pragma once

#include <array>
#include "Common.hpp"
#include "SearchLinear.hpp"

namespace blam::simd {
    template <std::array SignificantChars>
    BLAM_TARGET_SSE2 std::array<__m128i, SignificantChars.size() + 1> buildVCharsSSE2() noexcept {
        constexpr size_t N = SignificantChars.size();
        std::array<__m128i, N + 1> chars;
        chars[0] = _mm_set1_epi8('\n');
        for (size_t i = 1; i <= N; ++i) {
            chars[i] = _mm_set1_epi8(SignificantChars[i - 1]);
        }
        return chars;
    }

    template <std::array SignificantChars>
    BLAM_TARGET_SSE2 char const* skipNonSignificantSSE2(char const* ptr, char const* end) noexcept {
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

    BLAM_TARGET_SSE2 inline bool hasNonWhitespaceSSE2(char const* ptr, char const* end) noexcept {
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

    BLAM_TARGET_SSE2 inline char const* findAnyOfThreeSSE2(char const* ptr, char const* end, char a, char b, char c) noexcept {
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
    BLAM_TARGET_SSE2 SkipClassifiedResult skipNonSignificantClassifiedSSE2(char const* ptr, char const* end) noexcept {
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
}