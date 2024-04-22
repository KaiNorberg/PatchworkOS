#pragma once

#include "defs.h"

#include <common/boot_info.h>

#define PAGE_SIZE 0x1000

#define SIZE_IN_PAGES(size) (((size) + PAGE_SIZE - 1) / PAGE_SIZE)
#define PAGE_SIZE_OF(object) SIZE_IN_PAGES(sizeof(object))

typedef struct PageHeader
{
    struct PageHeader* next;
} PageHeader;

void pmm_init(EfiMemoryMap* efiMemoryMap);

void* pmm_allocate(void);

void pmm_free(void* address);

void pmm_free_pages(void* address, uint64_t count);

uint64_t pmm_total_amount(void);

uint64_t pmm_free_amount(void);

uint64_t pmm_reserved_amount(void);