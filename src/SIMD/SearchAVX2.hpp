#pragma once

#include <array>
#include "Common.hpp"
#include "SearchLinear.hpp"

namespace blam::simd {
    template <std::array SignificantChars>
    BLAM_TARGET_AVX2 std::array<__m256i, SignificantChars.size() + 1> buildVCharsAVX2() noexcept {
        constexpr size_t N = SignificantChars.size();
        std::array<__m256i, N + 1> chars;
        chars[0] = _mm256_set1_epi8('\n');
        for (size_t i = 1; i <= N; ++i) {
            chars[i] = _mm256_set1_epi8(SignificantChars[i - 1]);
        }
        return chars;
    }

    template <std::array SignificantChars>
    BLAM_TARGET_AVX2 char const* skipNonSignificantAVX2(char const* ptr, char const* end) noexcept {
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

    BLAM_TARGET_AVX2 inline bool hasNonWhitespaceAVX2(char const* ptr, char const* end) noexcept {
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

    BLAM_TARGET_AVX2 inline char const* findAnyOfThreeAVX2(char const* ptr, char const* end, char a, char b, char c) noexcept {
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

    template <std::array SignificantChars>
    BLAM_TARGET_AVX2 SkipClassifiedResult skipNonSignificantClassifiedAVX2(char const* ptr, char const* end) noexcept {
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
}