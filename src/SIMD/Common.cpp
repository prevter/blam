#include "Common.hpp"

#ifdef BLAM_PLATFORM_X86_64
    #if defined(_MSC_VER)
        #include <intrin.h>
    #elif defined(__GNUC__) || defined(__clang__)
        #include <cpuid.h>
        #include <x86intrin.h>
    #endif

    static uint64_t get_xcr0() noexcept {
    #if defined(_MSC_VER)
        return _xgetbv(0);
    #else
        uint32_t eax, edx;
        asm volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
        return (static_cast<uint64_t>(edx) << 32) | eax;
    #endif
    }
#elif defined(BLAM_PLATFORM_ARM64)
    #ifdef BLAM_PLATFORM_LINUX
        #include <asm/hwcap.h>
        #include <sys/auxv.h>
    #elif defined(BLAM_PLATFORM_MACOS)
        #include <sys/sysctl.h>
        #include <sys/types.h>
    #endif
#endif

namespace blam::simd {
    CPUFeatures detectCPUFeatures() noexcept {
        static CPUFeatures features = [] {
            CPUFeatures f;
        #ifdef BLAM_PLATFORM_X86_64
        #if defined(_MSC_VER)
            int info[4];
            __cpuid(info, 0);
            int nIds = info[0];
        #else
            unsigned int eax, ebx, ecx, edx;
            if (!__get_cpuid(0, &eax, &ebx, &ecx, &edx)) return f;
            int nIds = static_cast<int>(eax);
        #endif

            bool avx_cpu = false;
            bool avx_os  = false;

            if (nIds >= 1) {
            #if defined(_MSC_VER)
                __cpuid(info, 1);
                f.sse2  = (info[3] & (1 << 26)) != 0;
                avx_cpu = (info[2] & (1 << 28)) != 0;
                bool osxsave = (info[2] & (1 << 27)) != 0;
            #else
                __get_cpuid(1, &eax, &ebx, &ecx, &edx);
                f.sse2  = (edx & (1 << 26)) != 0;
                avx_cpu = (ecx & (1 << 28)) != 0;
                bool osxsave = (ecx & (1 << 27)) != 0;
            #endif

                if (avx_cpu && osxsave) {
                    uint64_t xcr0 = get_xcr0();
                    avx_os = (xcr0 & 0x6) == 0x6;
                }
            }

            if (nIds >= 7 && avx_cpu && avx_os) {
            #if defined(_MSC_VER)
                __cpuidex(info, 7, 0);
                f.avx2 = (info[1] & (1 << 5)) != 0;
            #else
                __get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx);
                f.avx2 = (ebx & (1 << 5)) != 0;
            #endif
            }
        #elif defined(BLAM_PLATFORM_ARM64)
            #ifdef BLAM_PLATFORM_LINUX
                auto hwcaps = getauxval(AT_HWCAP);
                f.neon = (hwcaps & HWCAP_ASIMD) != 0;

                // SVE is not implemented yet, so we'll just set it to false for now
                f.sve = false;

                // #ifdef HWCAP2_SVE2
                // auto hwcaps2 = getauxval(AT_HWCAP2);
                // f.sve = (hwcaps2 & HWCAP2_SVE2) != 0;
                // #elif defined(HWCAP_SVE)
                // f.sve = (hwcaps & HWCAP_SVE) != 0;
                // #else
                // f.sve = false;
                // #endif
            #elif defined(BLAM_PLATFORM_MACOS)
                // on macs, NEON is always supported
                f.neon = true;
                f.sve = false;
            #else
                f.neon = false;
                f.sve = false;
            #endif
        #endif
            return f;
        }();
        return features;
    }
}