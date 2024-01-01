#pragma once

#include <stdint.h>

#include "page_directory/page_directory.h"
#include "memory/memory.h"

#define GLOBAL_HEAP_BLOCK_MAX 64

typedef struct
{
    uint8_t present;
    void* physicalStart;
    void* virtualStart;
    uint64_t pageAmount;
    uint16_t pageFlags;
} GlobalHeapBlock;

void global_heap_init(EfiMemoryMap* memoryMap);

void global_heap_map(PageDirectory* pageDirectory);

void* gmalloc(uint64_t pageAmount, uint16_t flags);

void* gfree(uint64_t pageAmount, uint16_t flags);
