#pragma once

#include <stdint.h>

#include <efi.h>
#include <efilib.h>

#include <common/boot_info.h>

void* mem_alloc_pages(uint64_t pageAmount, uint64_t memoryType);

void* mem_alloc_pool(uint64_t size, uint64_t memoryType);

void mem_free_pool(void* pool);

void mem_map_init(efi_mem_map_t* memoryMap);

void mem_map_cleanup(efi_mem_map_t* memoryMap);
