#pragma once

#include <stdint.h>

#include <boot/boot_info.h>

#define HIGHER_HALF_BASE 0xFFFF800000000000

void vm_init(void);

void* vm_alloc_pages(void* virtAddr, uint64_t pageAmount, uint32_t type);

void* vm_alloc(uint64_t size);

void vm_map_init(efi_mem_map_t* memoryMap);
