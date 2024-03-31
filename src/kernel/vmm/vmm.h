#pragma once

#include <common/common.h>
#include <common/boot_info/boot_info.h>

#include "pmm/pmm.h"
#include "lock/lock.h"
#include "defs/defs.h"
#include "vmm/page_table/page_table.h"

#define VMM_HIGHER_HALF_BASE 0xFFFF800000000000
#define VMM_LOWER_HALF_MAX 0x800000000000

#define VMM_KERNEL_PAGE_FLAGS (PAGE_FLAG_GLOBAL)

#define VMM_HIGHER_TO_LOWER(address) \
    ((void*)((uint64_t)(address) - VMM_HIGHER_HALF_BASE))

#define VMM_LOWER_TO_HIGHER(address) \
    ((void*)((uint64_t)(address) + VMM_HIGHER_HALF_BASE))

typedef struct
{
    PageTable* pageTable;
    Lock lock;
} Space;

void vmm_init(EfiMemoryMap* memoryMap);

void* vmm_allocate(uint64_t pageAmount);

void* vmm_map(void* physicalAddress, uint64_t pageAmount, uint16_t flags);

void vmm_change_flags(void* address, uint64_t pageAmount, uint16_t flags);

Space* space_new(void);

void space_free(Space* space);

void space_load(Space* space);

void* space_allocate(Space* space, const void* address, uint64_t pageAmount);

void* space_physical_to_virtual(Space* space, const void* address);