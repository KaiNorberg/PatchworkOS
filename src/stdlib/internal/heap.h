#pragma once

#include <stdbool.h>
#include <stdint.h>

// TODO: Replace heap with a better algorithm, buddy allocator?

#define HEAP_ALIGNMENT 64
#define HEAP_BUCKET_AMOUNT 10

#define HEAP_HEADER_GET_START(block) ((void*)((uint64_t)block + sizeof(heap_header_t)))
#define HEAP_HEADER_GET_END(block) ((void*)((uint64_t)block + sizeof(heap_header_t) + block->size))
#define HEAP_HEADER_MAGIC 0xBC709F7DE48C8381

// Should be exactly 64 bytes long
typedef struct heap_header
{
    uint64_t magic;
    uint64_t size;
    uint64_t reserved;
    struct heap_header* next;
    uint64_t padding[4];
} heap_header_t;

heap_header_t* _HeapBlockNew(uint64_t size);

void _HeapBlockSplit(heap_header_t* block, uint64_t size);

heap_header_t* _HeapFirstBlock(void);

void _HeapInit(void);

void _HeapAcquire(void);

void _HeapRelease(void);
