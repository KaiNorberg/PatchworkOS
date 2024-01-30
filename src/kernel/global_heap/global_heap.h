#pragma once

#include <stdint.h>

#include "page_directory/page_directory.h"
#include "memory/memory.h"

void global_heap_init();

void global_heap_map(PageDirectory* pageDirectory);

void* gmalloc(uint64_t pageAmount);