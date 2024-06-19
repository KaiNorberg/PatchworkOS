#pragma once

#include <common/boot_info.h>

#include <sys/proc.h>

#include "defs.h"
#include "lock.h"
#include "pml.h"

#define VMM_HIGHER_HALF_BASE 0xFFFF800000000000
#define VMM_LOWER_HALF_MAX 0x800000000000

#define VMM_KERNEL_PAGES (PAGE_GLOBAL)

#define VMM_HIGHER_TO_LOWER(address) ((void*)((uint64_t)(address) - VMM_HIGHER_HALF_BASE))
#define VMM_LOWER_TO_HIGHER(address) ((void*)((uint64_t)(address) + VMM_HIGHER_HALF_BASE))

typedef struct
{
    pml_t* pml;
    uintptr_t freeAddress;
    lock_t lock;
} space_t;

void space_init(space_t* space);

void space_cleanup(space_t* space);

void space_load(space_t* space);

void vmm_init(efi_mem_map_t* memoryMap, gop_buffer_t* gopBuffer);

void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t length);

void* vmm_alloc(void* virtAddr, uint64_t length, prot_t prot);

void* vmm_map(void* virtAddr, void* physAddr, uint64_t length, prot_t prot);

uint64_t vmm_unmap(void* virtAddr, uint64_t length);

uint64_t vmm_protect(void* virtAddr, uint64_t length, prot_t prot);

bool vmm_mapped(const void* virtAddr, uint64_t length);
