#pragma once

#include "defs.h"

typedef struct
{
    uint8_t* buffer;
} simd_context_t;

void simd_init(void);

void simd_context_init(simd_context_t* context);

void simd_context_cleanup(simd_context_t* context);

void simd_context_save(simd_context_t* context);

void simd_context_load(simd_context_t* context);
