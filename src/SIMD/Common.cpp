#include "Common.hpp"

#ifdef BLAM_PLATFORM_X86_64

#if defined(_MSC_VER)
#include <intrin.h>
#endif

static void cpuid(int info[4], int infoType) noexcept {
    #if defined(_MSC_VER)
    __cpuid(info, infoType);
    #elif defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__(
        "cpuid"
        : "=a" (info[0]), "=b" (info[1]), "=c" (info[2]), "=d" (info[3])
        : "a" (infoType)
    );
    #endif
}
#elif defined(BLAM_PLATFORM_ARM64)
// #include <sys/auxv.h>
// #include <asm/hwcap.h>
#endif

namespace blam::simd {
    CPUFeatures detectCPUFeatures() noexcept {
        static CPUFeatures features = [] {
            CPUFeatures f;
        #ifdef BLAM_PLATFORM_X86_64
            int info[4];
            cpuid(info, 0);
            int nIds = info[0];

            bool avx_cpu = false;
            bool avx_os  = false;

            if (nIds >= 1) {
                cpuid(info, 1);
                f.sse2 = (info[3] & (1 << 26)) != 0;
                avx_cpu = (info[2] & (1 << 28)) != 0;
                bool osxsave = (info[2] & (1 << 27)) != 0;
                if (avx_cpu && osxsave) {
            #if defined(_MSC_VER)
                    uint64_t xcr0 = _xgetbv(0);
            #else
                    uint32_t eax, edx;
                    asm("xgetbv" : "=a"(eax), "=d"(edx) : "c"(0));
                    uint64_t xcr0 = (static_cast<uint64_t>(edx) << 32) | eax;
            #endif
                    avx_os = (xcr0 & 0x6) == 0x6;
                }
            }

            if (nIds >= 7 && avx_cpu && avx_os) {
                cpuid(info, 7);
                f.avx2 = (info[1] & (1 << 5)) != 0;
            }
        #elif defined(BLAM_PLATFORM_ARM64)
            // unsigned long hwcaps = getauxval(AT_HWCAP);
            // unsigned long hwcaps2 = getauxval(AT_HWCAP2);
            // f.neon = (hwcaps & HWCAP_ASIMD) != 0;
            // f.sve = (hwcaps2 & HWCAP2_SVE2) != 0;
            f.neon = false;
            f.sve = false;
        #endif
            return f;
        }();
        return features;
    }
}