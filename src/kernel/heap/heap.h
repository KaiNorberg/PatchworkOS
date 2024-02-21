#pragma once

#include <stdint.h>

#define HEAP_HEADER_GET_START(block) ((void*)((uint64_t)block + sizeof(HeapHeader)))
#define HEAP_HEADER_GET_END(block) ((void*)((uint64_t)block + sizeof(HeapHeader) + block->size))

typedef struct HeapHeader
{
    uint64_t size;
    uint8_t reserved;
    struct HeapHeader* next;
} HeapHeader;

void heap_init();

uint64_t heap_total_size();

uint64_t heap_reserved_size();

uint64_t heap_free_size();

void* kmalloc(uint64_t size);

void kfree(void* ptr);
