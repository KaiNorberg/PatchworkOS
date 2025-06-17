#pragma once

#include <stdint.h>

#define KALLOC_MAX_SLABS 64
#define KALLOC_MAX_SLAB_SIZE 0x64000

#define KALLOC_ALIGN 64

#define KALLOC_LOOKUP_NONE UINT8_MAX

typedef enum
{
    KALLOC_NONE = 0,
    KALLOC_VMM = 1 << 0,
} kalloc_flags_t;

void kalloc_init(void);

void* kmalloc(uint64_t size, kalloc_flags_t flags);
void* krealloc(void* oldPtr, uint64_t newSize, kalloc_flags_t flags);
void* kcalloc(uint64_t num, uint64_t size, kalloc_flags_t flags);

void kfree(void* ptr);