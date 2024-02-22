#pragma once

#include "kernel/kernel.h"
#include "page_directory/page_directory.h"
#include "pmm/pmm.h"

#include <common/common.h>

//#define USER_ADDRESS_SPACE_TOP 0x7FFFFFFFFFFF
//#define USER_ADDRESS_SPACE_BOTTOM 0

#define VMM_KERNEL_BASE 0xFFFFFFFF80000000
#define VMM_PHYSICAL_BASE 0xFFFF800000000000

#define VMM_KERNEL_PAGE_FLAGS (PAGE_FLAG_GLOBAL | PAGE_FLAG_DONT_FREE)

void vmm_init(EfiMemoryMap* memoryMap);

void* vmm_physical_to_virtual(void* address);

void* vmm_virtual_to_physical(void* address);

PageDirectory* vmm_kernel_directory();

void* vmm_allocate(uint64_t pageAmount, uint16_t flags);

void* vmm_map(void* physicalAddress, uint64_t pageAmount, uint16_t flags);

void vmm_map_kernel(PageDirectory* pageDirectory);
