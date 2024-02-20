#pragma once

#include "memory/memory.h"

#include <stdint.h>
#include <common/boot_info/boot_info.h>

#define SIZE_IN_PAGES(size) (((size) / 0x1000) + 1)

#define PAGE_SIZE_OF(object) SIZE_IN_PAGES(sizeof(object))

void pmm_init(EfiMemoryMap* memoryMap);

void* pmm_physical_base();

void pmm_move_to_higher_half();

void* pmm_allocate();

void* pmm_allocate_amount(uint64_t amount);

uint8_t pmm_is_reserved(void* address);

void pmm_lock_page(void* address);

void pmm_unlock_page(void* address);

void pmm_lock_pages(void* address, uint64_t count);

void pmm_unlock_pages(void* address, uint64_t count);

uint64_t pmm_unlocked_amount();

uint64_t pmm_locked_amount();

uint64_t pmm_total_amount();