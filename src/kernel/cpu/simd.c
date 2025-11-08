#include <kernel/cpu/cpu.h>
#include <kernel/cpu/simd.h>
#include <kernel/cpu/cpu.h>
#include <kernel/log/log.h>
#include <kernel/mem/pmm.h>

#include <kernel/defs.h>

#include <stdint.h>
#include <string.h>
#include <sys/cpuid.h>

static uint8_t initCtx[PAGE_SIZE] ALIGNED(64);

static void simd_xsave_init(void)
{
    cr4_write(cr4_read() | CR4_XSAVE_ENABLE);

    uint64_t xcr0 = 0;

    xcr0 = xcr0 | XCR0_XSAVE_SAVE_X87 | XCR0_XSAVE_SAVE_SSE;

    cpuid_feature_info_t info;
    cpuid_feature_info(&info);

    cpuid_extended_feature_info_t extInfo;
    cpuid_extended_feature_info(&extInfo);

    if (info.featuresEcx & CPUID_ECX_AVX)
    {
        xcr0 = xcr0 | XCR0_AVX_ENABLE;

        if (extInfo.featuresEbx & CPUID_EBX_AVX512F)
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

    cpuid_feature_info_t info;
    cpuid_feature_info(&info);

    cpuid_extended_feature_info_t extInfo;
    cpuid_extended_feature_info(&extInfo);

    if (info.featuresEcx & CPUID_ECX_OSXSAVE)
    {
        simd_xsave_init();
    }

    asm volatile("fninit");
    if (info.featuresEcx & CPUID_ECX_OSXSAVE)
    {
        asm volatile("xsave %0" : : "m"(*initCtx), "a"(UINT64_MAX), "d"(UINT64_MAX) : "memory");
    }
    else
    {
        asm volatile("fxsave (%0)" : : "r"(initCtx));
    }

    LOG_INFO("cpu%d simd ", cpu_get_unsafe()->id);
    if (info.featuresEcx & CPUID_ECX_OSXSAVE)
    {
        LOG_INFO("xsave ");
    }

    cpuid_instruction_sets_t sets = cpuid_detect_instruction_sets();
    if (sets & CPUID_INSTRUCTION_SET_SSE)
    {
        LOG_INFO("sse ");
    }
    if (sets & CPUID_INSTRUCTION_SET_SSE2)
    {
        LOG_INFO("sse2 ");
    }
    if (sets & CPUID_INSTRUCTION_SET_SSE3)
    {
        LOG_INFO("sse3 ");
    }
    if (sets & CPUID_INSTRUCTION_SET_SSSE3)
    {
        LOG_INFO("ssse3 ");
    }
    if (sets & CPUID_INSTRUCTION_SET_SSE4_1)
    {
        LOG_INFO("sse4.1 ");
    }
    if (sets & CPUID_INSTRUCTION_SET_SSE4_2)
    {
        LOG_INFO("sse4.2 ");
    }
    if (sets & CPUID_INSTRUCTION_SET_AVX)
    {
        LOG_INFO("avx ");
    }
    if (sets & CPUID_INSTRUCTION_SET_AVX2)
    {
        LOG_INFO("avx2 ");
    }
    if (sets & CPUID_INSTRUCTION_SET_AVX512)
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
    cpuid_feature_info_t info;
    cpuid_feature_info(&info);

    if (info.featuresEcx & CPUID_ECX_OSXSAVE)
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
    cpuid_feature_info_t info;
    cpuid_feature_info(&info);

    if (info.featuresEcx & CPUID_ECX_OSXSAVE)
    {
        asm volatile("xrstor %0" : : "m"(*ctx->buffer), "a"(UINT64_MAX), "d"(UINT64_MAX) : "memory");
    }
    else
    {
        asm volatile("fxrstor (%0)" : : "r"(ctx->buffer));
    }
}
