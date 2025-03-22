#pragma once

#include <stdbool.h>
#include <stdint.h>

// TODO: Replace heap with a better algorithm, buddy allocator?

#define _HEAP_ALIGNMENT 64

#define _HEAP_HEADER_GET_START(block) ((void*)((uint64_t)block + sizeof(_HeapHeader_t)))
#define _HEAP_HEADER_GET_END(block) ((void*)((uint64_t)block + sizeof(_HeapHeader_t) + block->size))
#define _HEAP_HEADER_MAGIC 0xBC709F7DE48C8381

// Should be exactly 64 bytes long
typedef struct _HeapHeader
{
    uint64_t magic;
    uint64_t size;
    uint64_t reserved;
    struct _HeapHeader* next;
    uint64_t padding[4];
} _HeapHeader_t;

_HeapHeader_t* _HeapBlockNew(uint64_t size);

void _HeapBlockSplit(_HeapHeader_t* block, uint64_t size);

_HeapHeader_t* _HeapFirstBlock(void);

void _HeapInit(void);

void _HeapAcquire(void);

void _HeapRelease(void);