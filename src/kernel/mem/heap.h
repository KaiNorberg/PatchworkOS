#pragma once

#include <stdint.h>

#define HEAP_MAX_SLABS 64
#define HEAP_MAX_SLAB_SIZE 0x64000

#define HEAP_ALIGN 64

#define HEAP_LOOKUP_NONE UINT8_MAX

#define HEAP_ALLOC_POISON 0xBAADF00D
#define HEAP_FREE_POISON  0xDEADC0DE

typedef enum
{
    HEAP_NONE = 0,
    HEAP_VMM = 1 << 0,
} heap_flags_t;

void heap_init(void);

void* heap_alloc(uint64_t size, heap_flags_t flags);
void* heap_realloc(void* oldPtr, uint64_t newSize, heap_flags_t flags);
void* heap_calloc(uint64_t num, uint64_t size, heap_flags_t flags);

void heap_free(void* ptr);
