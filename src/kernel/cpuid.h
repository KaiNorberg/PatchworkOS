#pragma once

#include "defs.h"

#define CPUID_FEATURE_ID 0x1
#define CPUID_FEATURE_EXTENDED_ID 0x7
#define CPUID_EXTENDED_STATE_ENUMERATION 0xD

#define CPUID_EBX_AVX512_AVAIL (1 << 16)

#define CPUID_ECX_XSAVE_AVAIL (1 << 26)
#define CPUID_ECX_AVX_AVAIL (1 << 28)

static inline void cpuid(uint32_t leaf, uint32_t subleaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx)
{
    asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(leaf), "c"(subleaf));
}

static inline bool cpuid_xsave_avail(void)
{
    uint32_t ecx;
    uint32_t unused;
    cpuid(CPUID_FEATURE_ID, 0, &unused, &unused, &ecx, &unused);
    return ecx & CPUID_ECX_XSAVE_AVAIL;
}

static inline bool cpuid_avx_avail(void)
{
    uint32_t ecx;
    uint32_t unused;
    cpuid(CPUID_FEATURE_ID, 0, &unused, &unused, &ecx, &unused);
    return ecx & CPUID_ECX_AVX_AVAIL;
}

static inline bool cpuid_avx512_avail(void)
{
    uint32_t eax;
    uint32_t ebx;
    uint32_t unused;
    cpuid(CPUID_FEATURE_EXTENDED_ID, 0, &eax, &ebx, &unused, &unused);
    return (eax != 0) && (ebx & CPUID_EBX_AVX512_AVAIL);
}
