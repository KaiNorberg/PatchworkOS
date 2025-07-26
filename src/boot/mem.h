#pragma once

#include <stdint.h>

#include <boot/boot_info.h>
#include <common/paging_types.h>

EFI_STATUS mem_page_table_init(page_table_t* table);

EFI_STATUS mem_map_init(boot_memory_map_t* map);

void mem_map_deinit(boot_memory_map_t* map);

EFI_STATUS mem_page_table_map_gop_kernel(page_table_t* table, boot_memory_map_t* map, boot_gop_t* gop,
    boot_kernel_t* kernel);
