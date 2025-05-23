#pragma once

#include "defs.h"

#include <bootloader/boot_info.h>

#include <sys/proc.h>

#define PMM_MAX_SPECIAL_ADDR (0x100000 * 64)

// Note: We free EFI_LOADER_DATA in kernel_init() after we are done copying data, so that is purposefully left out
#define PMM_IS_MEMORY_AVAIL(type) \
    ((type == EFI_CONVENTIONAL_MEMORY) || (type == EFI_PERSISTENT_MEMORY) || (type == EFI_LOADER_CODE) || \
        (type == EFI_BOOT_SERVICES_CODE) || (type == EFI_BOOT_SERVICES_DATA))

typedef struct page_buffer
{
    struct page_buffer* prev;
    void* pages[];
} page_buffer_t;

#define PMM_BUFFER_MAX ((PAGE_SIZE - sizeof(page_buffer_t)) / sizeof(void*))

typedef struct
{
    page_buffer_t* last;
    uint64_t index;
} pmm_stack_t;

#define PMM_BITMAP_MAX (PMM_MAX_SPECIAL_ADDR / PAGE_SIZE)

void pmm_init(efi_mem_map_t* memoryMap);

void* pmm_alloc(void);

void* pmm_alloc_bitmap(uint64_t count, uintptr_t maxAddr, uint64_t alignment);

void pmm_free(void* address);

void pmm_free_pages(void* address, uint64_t count);

uint64_t pmm_total_amount(void);

uint64_t pmm_free_amount(void);

uint64_t pmm_reserved_amount(void);
