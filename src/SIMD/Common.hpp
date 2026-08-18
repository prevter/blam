#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64)
    #define BLAM_PLATFORM_X86_64
    #include <immintrin.h>

    #if defined(__AVX2__)
        #define BLAM_HAS_AVX2
    #endif

    #if defined(__SSE2__)
        #define BLAM_HAS_SSE2
    #endif

    #if defined(__GNUC__) || defined(__clang__)
        #define BLAM_TARGET(x) __attribute__((target(x)))
    #else
        #define BLAM_TARGET(x)
    #endif
#elif defined(__aarch64__)
    #define BLAM_PLATFORM_ARM64

    #if defined(__ARM_NEON)
        #define BLAM_HAS_NEON
        #include <arm_neon.h>
    #endif

    #if defined(__ARM_FEATURE_SVE)
        #define BLAM_HAS_SVE
        #include <arm_sve.h>
    #endif
#endif

namespace blam::simd {
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