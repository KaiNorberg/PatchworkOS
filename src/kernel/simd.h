#pragma once

#include "defs.h"

typedef struct
{
    uint8_t* buffer;
} simd_ctx_t;

void simd_init(void);

uint64_t simd_ctx_init(simd_ctx_t* ctx);

void simd_ctx_deinit(simd_ctx_t* ctx);

void simd_ctx_save(simd_ctx_t* ctx);

void simd_ctx_load(simd_ctx_t* ctx);
