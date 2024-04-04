#pragma once

#include "defs/defs.h"

//TODO: Replace this with a better algorithm, buddy allocator?

#define HEAP_ALIGNMENT 64
#define HEAP_BUCKET_AMOUNT 10

#define HEAP_HEADER_GET_START(block) ((void*)((uint64_t)block + sizeof(HeapHeader)))
#define HEAP_HEADER_GET_END(block) ((void*)((uint64_t)block + sizeof(HeapHeader) + block->size))
#define HEAP_HEADER_MAGIC 0xBC709F7DE48C8381

//Should be exactly 64 bytes long
typedef struct HeapHeader
{
    uint64_t magic;
    uint64_t size;
    uint64_t reserved;
    struct HeapHeader* next;
    uint64_t padding[4];
} HeapHeader;

void heap_init(void);

uint64_t heap_total_size(void);

uint64_t heap_reserved_size(void);

uint64_t heap_free_size(void);

void* kmalloc(uint64_t size);

void* kcalloc(uint64_t count, uint64_t size);

void kfree(void* ptr);
