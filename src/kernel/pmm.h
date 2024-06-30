#pragma once

#include "defs.h"

#include <bootloader/boot_info.h>

#include <sys/proc.h>

typedef struct page_header
{
    struct page_header* next;
} page_header_t;

void pmm_init(efi_mem_map_t* memoryMap);

void* pmm_alloc(void);

void pmm_free(void* address);

void pmm_free_pages(void* address, uint64_t count);

uint64_t pmm_total_amount(void);

uint64_t pmm_free_amount(void);

uint64_t pmm_reserved_amount(void);

const char* pmm_mem_type_to_string(uint32_t type);
