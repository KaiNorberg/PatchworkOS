#pragma once

#include "defs.h"

#define CPUID_REQ_FEATURE 1
#define CPUID_REQ_FEATURE_EXTENDED 7

#define CPUID_EBX_AVX512_AVAIL (1 << 16)

#define CPUID_ECX_XSAVE_AVAIL (1 << 26)
#define CPUID_ECX_AVX_AVAIL (1 << 28)

static inline void cpuid(uint32_t code, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx)
{
    asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "a"(code));
}

static inline bool cpuid_xsave_avail(void)
{
    uint32_t ecx;
    uint32_t unused;
    cpuid(CPUID_REQ_FEATURE, &unused, &unused, &ecx, &unused);
    return ecx & CPUID_ECX_XSAVE_AVAIL;
}

static inline bool cpuid_avx_avail(void)
{
    uint32_t ecx;
    uint32_t unused;
    cpuid(CPUID_REQ_FEATURE, &unused, &unused, &ecx, &unused);
    return ecx & CPUID_ECX_AVX_AVAIL;
}

static inline bool cpuid_avx512_avail(void)
{
    uint32_t ebx;
    uint32_t unused;
    cpuid(CPUID_REQ_FEATURE_EXTENDED, &unused, &ebx, &unused, &unused);
    return ebx & CPUID_EBX_AVX512_AVAIL;
}