#pragma once

#include <stdint.h>

#include <boot/boot_info.h>
#include <common/paging_types.h>

#define MEM_INITAL_MAP_CAPACITY 256
#define MEM_BASIC_ALLOCATOR_MAX_PAGES 1024

EFI_STATUS mem_init(void);

EFI_STATUS mem_map_init(boot_memory_map_t* map);

void mem_map_deinit(boot_memory_map_t* map);

void mem_page_table_init(page_table_t* table, boot_memory_map_t* map, boot_gop_t* gop, boot_kernel_t* kernel);
