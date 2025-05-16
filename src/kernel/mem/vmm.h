#pragma once

#include "defs.h"
#include "sync/lock.h"
#include "pml.h"

#include <bootloader/boot_info.h>

#include <sys/proc.h>

#define VMM_HIGHER_HALF_BASE 0xFFFF800000000000
#define VMM_LOWER_HALF_MAX 0x800000000000

#define VMM_KERNEL_PAGES (PAGE_GLOBAL)

#define VMM_HIGHER_TO_LOWER(address) ((void*)((uint64_t)(address) - VMM_HIGHER_HALF_BASE))
#define VMM_LOWER_TO_HIGHER(address) ((void*)((uint64_t)(address) + VMM_HIGHER_HALF_BASE))

void vmm_init(efi_mem_map_t* memoryMap, boot_kernel_t* kernel, gop_buffer_t* gopBuffer);

void vmm_cpu_init(void);

pml_t* vmm_kernel_pml(void);

void* vmm_kernel_alloc(void* virtAddr, uint64_t length);

void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t length);

void* vmm_alloc(void* virtAddr, uint64_t length, prot_t prot);

void* vmm_map(void* virtAddr, void* physAddr, uint64_t length, prot_t prot);

uint64_t vmm_unmap(void* virtAddr, uint64_t length);

uint64_t vmm_protect(void* virtAddr, uint64_t length, prot_t prot);

bool vmm_mapped(const void* virtAddr, uint64_t length);
