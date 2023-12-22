#pragma once

#include <stdint.h>

#include "page_allocator/page_allocator.h"
#include "page_directory/page_directory.h"

void heap_init();

void heap_visualize();

uint64_t heap_total_size();

uint64_t heap_reserved_size();

uint64_t heap_free_size();

uint64_t heap_block_count();

void* kmalloc(uint64_t size);

void kfree(void* ptr);
