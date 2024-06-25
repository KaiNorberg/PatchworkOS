#include "simd.h"

#include "cpuid.h"
#include "pmm.h"
#include "regs.h"
#include "vmm.h"

#include <string.h>
#include <stdlib.h>

static uint8_t initContext[PAGE_SIZE];

static uint64_t contextSize;

static void simd_xsave_init(void)
{
    cr4_write(cr4_read() | CR4_XSAVE_ENABLE);

    uint64_t xcr0 = 0;

    xcr0 = xcr0 | XCR0_XSAVE_SAVE_X87 | XCR0_XSAVE_SAVE_SSE;

    if (cpuid_avx_avail())
    {
        xcr0 = xcr0 | XCR0_AVX_ENABLE;

        if (cpuid_avx512_avail())
        {
            xcr0 = xcr0 | XCR0_AVX512_ENABLE | XCR0_ZMM0_15_ENABLE | XCR0_ZMM16_32_ENABLE;
        }
    }

    xcr0_write(0, xcr0);
}

void simd_init(void)
{
    cr0_write(cr0_read() & ~((uint64_t)CR0_EMULATION));
    cr0_write(cr0_read() | CR0_MONITOR_CO_PROCESSOR | CR0_NUMERIC_ERROR_ENABLE);

    cr4_write(cr4_read() | CR4_FXSR_ENABLE | CR4_SIMD_EXCEPTION);

    if (cpuid_xsave_avail())
    {
        simd_xsave_init();

        contextSize = cpuid_xsave_size();
    }
    else
    {
        contextSize = 512;
    }

    asm volatile("fninit");
    if (cpuid_xsave_avail())
    {
        asm volatile("xsave %0" ::"m"(*initContext), "a"(UINT64_MAX), "d"(UINT64_MAX) : "memory");
    }
    else
    {
        asm volatile("fxsave (%0)" ::"a"(initContext));
    }
}

void simd_context_init(simd_context_t* context)
{
    context->buffer = malloc(contextSize);
    memcpy(context->buffer, initContext, contextSize);
}

void simd_context_cleanup(simd_context_t* context)
{
    free(context->buffer);
}

void simd_context_save(simd_context_t* context)
{
    if (cpuid_xsave_avail())
    {
        asm volatile("xsave %0" ::"m"(*context->buffer), "a"(UINT64_MAX), "d"(UINT64_MAX) : "memory");
    }
    else
    {
        asm volatile("fxsave (%0)" ::"a"(context->buffer));
    }
}

void simd_context_load(simd_context_t* context)
{
    if (cpuid_xsave_avail())
    {
        asm volatile("xrstor %0" ::"m"(*context->buffer), "a"(UINT64_MAX), "d"(UINT64_MAX) : "memory");
    }
    else
    {
        asm volatile("fxrstor (%0)" ::"a"(context->buffer));
    }
}
