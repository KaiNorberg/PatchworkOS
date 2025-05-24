#pragma once

#include "defs.h"
#include "pml.h"
#include "sync/lock.h"
#include "utils/bitmap.h"

#include <bootloader/boot_info.h>

#include <sys/list.h>
#include <sys/proc.h>

#define VMM_HIGHER_HALF_BASE 0xFFFF800000000000
#define VMM_LOWER_HALF_MAX 0x800000000000

#define VMM_KERNEL_PAGES (PAGE_GLOBAL)

#define VMM_HIGHER_TO_LOWER(address) ((void*)((uint64_t)(address) - VMM_HIGHER_HALF_BASE))
#define VMM_LOWER_TO_HIGHER(address) ((void*)((uint64_t)(address) + VMM_HIGHER_HALF_BASE))

// Allows a physical address to be specified in either the upper or lower half
#define VMM_PHYS_TO_LOWER_SAFE(address) \
    (address >= (void*)VMM_HIGHER_HALF_BASE ? VMM_HIGHER_TO_LOWER(address) : address)

typedef void (*vmm_callback_t)(void* private);

// Stores regions that have a assigned callback so that the callback can be called when it is unmapped
typedef struct
{
    list_entry_t entry;
    vmm_callback_t callback;
    void* private;
    void* start;
    bitmap_t pages; // The length of the bitmap (amount of bits in the bitmap) is the amount of pages it tracks. 1 means
                    // still mapped, 0 means it has been unmapped.
    uint8_t bitBuffer[]; // Stores the bitmap data
} mapped_region_t;

typedef struct
{
    pml_t* pml;
    uintptr_t freeAddress;
    list_t mappedRegions;
    lock_t lock;
} space_t;

void space_init(space_t* space);

void space_deinit(space_t* space);

void space_load(space_t* space);

void vmm_init(efi_mem_map_t* memoryMap, boot_kernel_t* kernel, gop_buffer_t* gopBuffer);

void vmm_cpu_init(void);

pml_t* vmm_kernel_pml(void);

void* vmm_kernel_alloc(void* virtAddr, uint64_t length);

void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t length);

void* vmm_alloc(void* virtAddr, uint64_t length, prot_t prot);

// Calls the callback when the entire mapped area has been unmapped or when the space is freed.
void* vmm_map(void* virtAddr, void* physAddr, uint64_t length, prot_t prot, vmm_callback_t callback, void* private);

// Calls the callback when the entire mapped area has been unmapped or when the space is freed.
void* vmm_map_pages(void* virtAddr, void** pages, uint64_t pageAmount, prot_t prot, vmm_callback_t callback,
    void* private);

uint64_t vmm_unmap(void* virtAddr, uint64_t length);

uint64_t vmm_protect(void* virtAddr, uint64_t length, prot_t prot);

bool vmm_mapped(const void* virtAddr, uint64_t length);
