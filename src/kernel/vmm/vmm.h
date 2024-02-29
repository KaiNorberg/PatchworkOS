#pragma once

#include <stdint.h>

#include <common/common.h>
#include <common/boot_info/boot_info.h>

#include "kernel/kernel.h"
#include "page_directory/page_directory.h"
#include "pmm/pmm.h"

#define VMM_HIGHER_HALF_BASE 0xFFFF800000000000
#define VMM_LOWER_HALF_MAX 0x7FFFFFFFF000

#define VMM_KERNEL_PAGE_FLAGS (PAGE_FLAG_GLOBAL | PAGE_FLAG_DONT_OWN)

void vmm_init(EfiMemoryMap* memoryMap);

PageDirectory* vmm_kernel_directory();

void* vmm_physical_to_virtual(void* address);

void* vmm_virtual_to_physical(void* address);

void* vmm_allocate(uint64_t pageAmount);

void vmm_free(void* address, uint64_t pageAmount);

void* vmm_map(void* physicalAddress, uint64_t pageAmount, uint16_t flags);

void vmm_change_flags(void* address, uint64_t pageAmount, uint16_t flags);

void vmm_map_kernel(PageDirectory* pageDirectory);