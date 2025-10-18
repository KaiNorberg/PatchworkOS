#include "simd.h"
#include "smp.h"
#include "log/log.h"
#include "mem/pmm.h"
#include "cpu.h"

#include <common/defs.h>

#include <stdint.h>
#include <string.h>
#include <sys/cpuid.h>

static uint8_t initCtx[PAGE_SIZE] ALIGNED(64);

static void simd_xsave_init(void)
{
    cr4_write(cr4_read() | CR4_XSAVE_ENABLE);

    uint64_t xcr0 = 0;

    xcr0 = xcr0 | XCR0_XSAVE_SAVE_X87 | XCR0_XSAVE_SAVE_SSE;

    if (cpuid_is_avx_avail())
    {
        xcr0 = xcr0 | XCR0_AVX_ENABLE;

        if (cpuid_is_avx512_avail())
        {
            xcr0 = xcr0 | XCR0_AVX512_ENABLE | XCR0_ZMM0_15_ENABLE | XCR0_ZMM16_32_ENABLE;
        }
    }

    xcr0_write(0, xcr0);
}

void simd_cpu_init(void)
{
    cr0_write(cr0_read() & ~((uint64_t)CR0_EMULATION));
    cr0_write(cr0_read() | CR0_MONITOR_CO_PROCESSOR | CR0_NUMERIC_ERROR_ENABLE);

    cr4_write(cr4_read() | CR4_FXSR_ENABLE | CR4_SIMD_EXCEPTION);

    if (cpuid_is_xsave_avail())
    {
        simd_xsave_init();
    }

    asm volatile("fninit");
    if (cpuid_is_xsave_avail())
    {
        asm volatile("xsave %0" : : "m"(*initCtx), "a"(UINT64_MAX), "d"(UINT64_MAX) : "memory");
    }
    else
    {
        asm volatile("fxsave (%0)" : : "r"(initCtx));
    }

    LOG_INFO("cpu%d simd ", smp_self_unsafe()->id);
    if (cpuid_is_xsave_avail())
    {
        LOG_INFO("xsave ");
    }
    if (cpuid_is_avx_avail())
    {
        LOG_INFO("avx ");
    }
    if (cpuid_is_avx512_avail())
    {
        LOG_INFO("avx512 ");
    }
    LOG_INFO("enabled\n");
}

uint64_t simd_ctx_init(simd_ctx_t* ctx)
{
    ctx->buffer = pmm_alloc();
    if (ctx->buffer == NULL)
    {
        return ERR;
    }

    memcpy(ctx->buffer, initCtx, PAGE_SIZE);

    return 0;
}

void simd_ctx_deinit(simd_ctx_t* ctx)
{
    pmm_free(ctx->buffer);
}

void simd_ctx_save(simd_ctx_t* ctx)
{
    if (cpuid_is_xsave_avail())
    {
        asm volatile("xsave %0" : : "m"(*ctx->buffer), "a"(UINT64_MAX), "d"(UINT64_MAX) : "memory");
    }
    else
    {
        asm volatile("fxsave (%0)" : : "r"(ctx->buffer));
    }
}

void simd_ctx_load(simd_ctx_t* ctx)
{
    if (cpuid_is_xsave_avail())
    {
        asm volatile("xrstor %0" : : "m"(*ctx->buffer), "a"(UINT64_MAX), "d"(UINT64_MAX) : "memory");
    }
    else
    {
        asm volatile("fxrstor (%0)" : : "r"(ctx->buffer));
    }
}
