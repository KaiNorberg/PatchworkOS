#pragma once

#include <stdint.h>

#include <common/boot_info/boot_info.h>

#define HIGHER_HALF_BASE 0xFFFF800000000000

#define ALLOCATED_ADDRESS_MAX_AMOUNT 16

typedef struct
{
    void* physicalAddress;
    void* virtualAddress;
} AllocatedAddress;

void virtual_memory_init();

void virtual_memory_allocate_address(void* virtualAddress, uint64_t pageAmount, uint64_t memoryType);

void* virtual_memory_allocate_pages(uint64_t pageAmount, uint64_t memoryType);

void* virtual_memory_allocate_pool(uint64_t size, uint64_t memoryType);

void virtual_memory_map_populate(EfiMemoryMap* memoryMap);
