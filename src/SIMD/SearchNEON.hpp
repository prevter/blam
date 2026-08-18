#pragma once

#include <array>
#include "Common.hpp"
#include "SearchLinear.hpp"

namespace blam::simd {
    BLAM_TARGET_NEON inline uint16_t neon_movemask(uint8x16_t v) noexcept {
        uint8x16_t vbits = vshrq_n_u8(v, 7);
        uint64x2_t v64 = vreinterpretq_u64_u8(vbits);
        uint64_t m0 = vgetq_lane_u64(v64, 0);
        uint64_t m1 = vgetq_lane_u64(v64, 1);
        uint64_t bits0 = (m0 & 0x0101010101010101ULL) * 0x0102040810204080ULL;
        uint64_t bits1 = (m1 & 0x0101010101010101ULL) * 0x0102040810204080ULL;
        uint16_t mask = ((bits0 >> 56) & 0xFF) | (((bits1 >> 56) & 0xFF) << 8);
        return mask;
    }

    template <std::array SignificantChars>
    BLAM_TARGET_NEON std::array<uint8x16_t, SignificantChars.size() + 1> buildVCharsNEON() noexcept {
        constexpr size_t N = SignificantChars.size();
        std::array<uint8x16_t, N + 1> chars;
        chars[0] = vdupq_n_u8('\n');
        for (size_t i = 0; i < N; ++i) {
            chars[i + 1] = vdupq_n_u8(SignificantChars[i]);
        }
        return chars;
    }

    template <std::array SignificantChars>
    BLAM_TARGET_NEON char const* skipNonSignificantNEON(char const* ptr, char const* end) noexcept {
        constexpr size_t N = SignificantChars.size();
        static auto const vchars = buildVCharsNEON<SignificantChars>();
        while (ptr + 16 <= end) {
            uint8x16_t chunk = vld1q_u8(reinterpret_cast<uint8_t const*>(ptr));
            uint8x16_t cmp = vceqq_u8(chunk, vchars[0]);
            for (size_t i = 1; i <= N; ++i) {
                cmp = vorrq_u8(cmp, vceqq_u8(chunk, vchars[i]));
            }

            uint16_t mask = neon_movemask(cmp);
            if (mask != 0) {
                return ptr + __builtin_ctz(mask);
            }

            ptr += 16;
        }

        return skipNonSignificantLinear<SignificantChars>(ptr, end);
    }

    BLAM_TARGET_NEON inline bool hasNonWhitespaceNEON(char const* ptr, char const* end) noexcept {
        uint8x16_t vspace = vdupq_n_u8(' ');
        uint8x16_t vtab = vdupq_n_u8('\t');
        uint8x16_t vcr = vdupq_n_u8('\r');

        while (ptr + 16 <= end) {
            uint8x16_t chunk = vld1q_u8(reinterpret_cast<uint8_t const*>(ptr));
            uint8x16_t cmp = vorrq_u8(
                vorrq_u8(vceqq_u8(chunk, vspace), vceqq_u8(chunk, vtab)),
                vceqq_u8(chunk, vcr)
            );

            uint16_t mask = neon_movemask(cmp);
            if (mask != 0xFFFF) return true;

            ptr += 16;
        }

        return hasNonWhitespaceLinear(ptr, end);
    }

    BLAM_TARGET_NEON inline char const* findAnyOfThreeNEON(char const* ptr, char const* end, char a, char b, char c) noexcept {
        uint8x16_t va = vdupq_n_u8(a);
        uint8x16_t vb = vdupq_n_u8(b);
        uint8x16_t vc = vdupq_n_u8(c);

        while (ptr + 16 <= end) {
            uint8x16_t chunk = vld1q_u8(reinterpret_cast<uint8_t const*>(ptr));
            uint8x16_t cmp = vorrq_u8(
                vorrq_u8(vceqq_u8(chunk, va), vceqq_u8(chunk, vb)),
                vceqq_u8(chunk, vc)
            );

            uint16_t mask = neon_movemask(cmp);
            if (mask != 0) {
                return ptr + __builtin_ctz(mask);
            }

            ptr += 16;
        }

        return findAnyOfThreeLinear(ptr, end, a, b, c);
    }

    template <std::array SignificantChars>
    BLAM_TARGET_NEON SkipClassifiedResult skipNonSignificantClassifiedNEON(char const* ptr, char const* end) noexcept {
        constexpr size_t N = SignificantChars.size();

        static auto const vchars = buildVCharsNEON<SignificantChars>();
        uint8x16_t vsp = vdupq_n_u8(' ');
        uint8x16_t vtab = vdupq_n_u8('\t');
        uint8x16_t vcr = vdupq_n_u8('\r');
        bool sawNonWs = false;

        while (ptr + 16 <= end) {
            uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(ptr));
            uint8x16_t sigCmp = vceqq_u8(chunk, vchars[0]);
            for (size_t i = 1; i <= N; ++i) {
                sigCmp = vorrq_u8(sigCmp, vceqq_u8(chunk, vchars[i]));
            }

            uint16_t sigMask = neon_movemask(sigCmp);
            uint8x16_t wsCmp = vorrq_u8(
                vorrq_u8(vceqq_u8(chunk, vsp), vceqq_u8(chunk, vtab)),
                vceqq_u8(chunk, vcr)
            );

            uint16_t wsMask = neon_movemask(wsCmp);

            if (sigMask != 0) {
                uint32_t pos = __builtin_ctz(sigMask);
                if (pos > 0) {
                    uint16_t prefix = (1u << pos) - 1;
                    if ((wsMask & prefix) != prefix) sawNonWs = true;
                }
                return {ptr + pos, sawNonWs};
            }

            if (wsMask != 0xFFFF) sawNonWs = true;

            ptr += 16;
        }

        auto [fp, fNw] = skipNonSignificantClassifiedLinear<SignificantChars>(ptr, end);
        return {fp, static_cast<bool>(sawNonWs | fNw)};
    }
}