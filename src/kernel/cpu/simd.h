#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief SIMD context management
 * @defgroup kernel_cpu_simd SIMD
 * @ingroup kernel_cpu
 *
 * SIMD (Single Instruction, Multiple Data) context management allows saving and restoring the state of SIMD registers,
 * the fact that SIMD uses its own registers is the reason that we cant use SIMD in the kernel normally.
 *
 * @see [XSAVE Instruction](https://www.felixcloutier.com/x86/xsave)
 * @see [FXSAVE Instruction](https://www.felixcloutier.com/x86/fxsave)
 * @see [FNINIT Instruction](https://www.felixcloutier.com/x86/fninit)
 *
 * @{
 */

typedef struct
{
    uint8_t* buffer;
} simd_ctx_t;

void simd_cpu_init(void);

uint64_t simd_ctx_init(simd_ctx_t* ctx);

void simd_ctx_deinit(simd_ctx_t* ctx);

void simd_ctx_save(simd_ctx_t* ctx);

void simd_ctx_load(simd_ctx_t* ctx);

/** @} */
