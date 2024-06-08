#pragma once

#include "defs.h"

#include <common/boot_info.h>

#include <sys/proc.h>

typedef struct PageHeader
{
    struct PageHeader* next;
} PageHeader;

void pmm_init(EfiMemoryMap* efiMemoryMap);

void* pmm_alloc(void);

void pmm_free(void* address);

void pmm_free_pages(void* address, uint64_t count);

uint64_t pmm_total_amount(void);

uint64_t pmm_free_amount(void);

uint64_t pmm_reserved_amount(void);