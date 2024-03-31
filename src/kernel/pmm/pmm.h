#pragma once

#include "defs/defs.h"

#include <common/boot_info/boot_info.h>

//TODO: Implement lazy loading of the memory map?

#define PAGE_SIZE 0x1000

#define SIZE_IN_PAGES(size) \
    (((size) / PAGE_SIZE) + 1)

#define PAGE_SIZE_OF(object) \
    SIZE_IN_PAGES(sizeof(object))

typedef struct PageHeader
{
    struct PageHeader* next;
} PageHeader;

void pmm_init(EfiMemoryMap* memoryMap);

void* pmm_allocate(void);

void pmm_free(void* address);

void pmm_free_pages(void* address, uint64_t count);

uint64_t pmm_total_amount(void);

uint64_t pmm_free_amount(void);

uint64_t pmm_reserved_amount(void);