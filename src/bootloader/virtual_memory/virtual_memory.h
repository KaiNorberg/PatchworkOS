#pragma once

#include <stdint.h>

#include <common/boot_info/boot_info.h>

#define HIGHER_HALF_BASE 0xFFFF800000000000

void virtual_memory_init(void);

void virtual_memory_allocate_kernel(void* virtualAddress, uint64_t pageAmount);

void* virtual_memory_allocate_pages(uint64_t pageAmount, uint64_t memoryType);

void* virtual_memory_allocate_pool(uint64_t size, uint64_t memoryType);

void virtual_memory_map_init(EfiMemoryMap* memoryMap);
