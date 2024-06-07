#pragma once

#include "defs.h"

typedef struct
{
    uint8_t* buffer;
} SimdContext;

void simd_init(void);

void simd_context_init(SimdContext* context);

void simd_context_cleanup(SimdContext* context);

void simd_context_save(SimdContext* context);

void simd_context_load(SimdContext* context);