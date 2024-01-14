#pragma once

#include "memory/memory.h"

#include "gop/gop.h"

#include <stdint.h>

#define GET_SIZE_IN_PAGES(size) (((size) / 0x1000) + 1)

void page_allocator_visualize();

void page_allocator_init(EfiMemoryMap* memoryMap);

void* page_allocator_request();

void* page_allocator_request_amount(uint64_t amount);

uint8_t page_allocator_get_status(void* address);

void page_allocator_lock_page(void* address);

void page_allocator_unlock_page(void* address);

void page_allocator_lock_pages(void* address, uint64_t count);

void page_allocator_unlock_pages(void* address, uint64_t count);

uint64_t page_allocator_unlocked_amount();

uint64_t page_allocator_locked_amount();

uint64_t page_allocator_get_total_amount();