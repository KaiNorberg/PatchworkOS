#pragma once

#include "defs.h"

typedef struct
{
    uint8_t* buffer;
} simd_context_t;

void simd_init(void);

uint64_t simd_context_init(simd_context_t* context);

void simd_context_deinit(simd_context_t* context);

void simd_context_save(simd_context_t* context);

void simd_context_load(simd_context_t* context);
