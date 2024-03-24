#pragma once

#include <common/common.h>
#include <common/boot_info/boot_info.h>

#include "pmm/pmm.h"
#include "lock/lock.h"
#include "types/types.h"
#include "vmm/page_directory/page_directory.h"

#define VMM_HIGHER_HALF_BASE 0xFFFF800000000000
#define VMM_LOWER_HALF_MAX 0x7FFFFFFFF000

#define VMM_KERNEL_PAGE_FLAGS (PAGE_FLAG_GLOBAL)

typedef struct
{
    PageDirectory* pageDirectory;
    Lock lock;
} AddressSpace;

void vmm_init(EfiMemoryMap* memoryMap);

void* vmm_physical_to_virtual(void* address);

void* vmm_virtual_to_physical(void* address);

void* vmm_map(void* physicalAddress, uint64_t pageAmount, uint16_t flags);

void vmm_change_flags(void* address, uint64_t pageAmount, uint16_t flags);

AddressSpace* address_space_new(void);

void address_space_free(AddressSpace* space);

void address_space_load(AddressSpace* space);

void* address_space_allocate(AddressSpace* space, void* address, uint64_t pageAmount);