#pragma once

#include <stdint.h>

#include <common/boot_info.h>

#define HIGHER_HALF_BASE 0xFFFF800000000000

void vm_init(void);

void vm_alloc_kernel(void* virtAddr, uint64_t pageAmount);

void* vm_alloc(uint64_t size, uint64_t memoryType);

void vm_map_init(EfiMemoryMap* memoryMap);
