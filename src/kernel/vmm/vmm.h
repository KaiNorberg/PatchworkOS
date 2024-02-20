#pragma once

#include "kernel/kernel.h"
#include "page_directory/page_directory.h"
#include "pmm/pmm.h"

#include <common/common.h>

#define VMM_KERNEL_BASE 0xFFFFFFFF80000000
#define VMM_PHYSICAL_BASE 0xFFFF800000000000

void vmm_init(EfiMemoryMap* memoryMap);

void* vmm_physical_to_virtual(void* address);

PageDirectory* vmm_kernel_directory();

void* vmm_request_memory(uint64_t pageAmount, uint16_t flags);

void* vmm_request_address(void* physicalAddress, uint64_t pageAmount, uint16_t flags);

void vmm_map_kernel(PageDirectory* pageDirectory);
