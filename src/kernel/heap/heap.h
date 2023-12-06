#pragma once

#include <stdint.h>

#include "kernel/page_allocator/page_allocator.h"
#include "kernel/virtual_memory/virtual_memory.h"

void heap_init(VirtualAddressSpace* addressSpace, uint64_t heapStart, uint64_t startSize);

void heap_visualize();

uint64_t heap_total_size();

uint64_t heap_reserved_size();

uint64_t heap_free_size();

uint64_t heap_block_count();

void heap_reserve(uint64_t size);

void* kmalloc(uint64_t size);

void kfree(void* ptr);
