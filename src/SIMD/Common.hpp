#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
    #define WTF_PLATFORM_X86_64
    #include <immintrin.h>

    #if defined(__AVX2__)
        #define WTF_HAS_AVX2
    #endif

    #if defined(__SSE2__)
        #define WTF_HAS_SSE2
    #endif

    #if defined(__GNUC__) || defined(__clang__)
        #define WTF_TARGET(x) __attribute__((target(x)))
    #else
        #define WTF_TARGET(x)
    #endif
#elif defined(__aarch64__)
    #define WTF_PLATFORM_ARM64

    #if defined(__ARM_NEON)
        #define WTF_HAS_NEON
    #endif

    #if defined(__ARM_FEATURE_SVE)
        #define WTF_HAS_SVE
    #endif
#endif

namespace wtf::simd {
    struct CPUFeatures {
        // X86
        bool sse2 = false;
        bool avx2 = false;

        // ARM
        bool neon = false;
        bool sve = false;
    };

    CPUFeatures detectCPUFeatures() noexcept;
}