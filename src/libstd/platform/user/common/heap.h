#pragma once

#include <stdbool.h>
#include <stdint.h>

// TODO: Replace user heap with a better algorithm, buddy allocator?

#define _HEAP_ALIGNMENT 64

#define _HEAP_HEADER_GET_START(block) ((void*)((uint64_t)block + sizeof(_heap_header_t)))
#define _HEAP_HEADER_GET_END(block) ((void*)((uint64_t)block + sizeof(_heap_header_t) + block->size))
#define _HEAP_HEADER_MAGIC 0xBC709F7DE48C8381

// Should be exactly 64 bytes long
typedef struct _heap_header
{
    uint64_t magic;
    uint64_t size;
    uint64_t reserved;
    struct _heap_header* next;
    uint64_t padding[4];
} _heap_header_t;

_heap_header_t* _heap_block_new(uint64_t size);

void _heap_block_split(_heap_header_t* block, uint64_t size);

void _heap_init(void);

_heap_header_t* _heap_first_block(void);

void* _heap_alloc(uint64_t size);

void _heap_free(void* ptr);

void _heap_acquire(void);

void _heap_release(void);
