#include "pmm.h"

#include <stddef.h>
#include <string.h>

#include <common/boot_info.h>

#include "config.h"
#include "debug.h"
#include "lock.h"
#include "log.h"
#include "vmm.h"

static const char* efiMemTypeToString[] =
{
    "efi reserved memory type",
    "efi loader code",
    "efi loader data",
    "efi boot services code",
    "efi boot services data",
    "efi runtime services code",
    "efi runtime services data",
    "efi conventional memory",
    "efi unusable memory",
    "efi acpi reclaim memory",
    "efi acpi memory nvs",
    "efi memory mapped io",
    "efi memory mapped io port space",
    "efi pal code",
    "efi persistent memory"
};

static const char* kernelMemTypeToString[] =
{
    "kernel memory",
    "pml memory",
    "boot info",
    "ram disk",
    "memory map",
};

static page_header_t* firstPage = NULL;
static page_header_t* lastPage = NULL;
static uint64_t pageAmount = 0;
static uint64_t loadedPageAmount = 0;
static uint64_t freePageAmount = 0;

static efi_mem_map_t memoryMap;

static lock_t lock;

static uint8_t pmm_is_type_free(uint64_t memoryType)
{
    return memoryType == EFI_CONVENTIONAL_MEMORY || memoryType == EFI_LOADER_CODE || memoryType == EFI_LOADER_DATA ||
        memoryType == EFI_BOOT_SERVICES_CODE || memoryType == EFI_BOOT_SERVICES_DATA;
}

static const char* pmm_mem_type_to_string(uint32_t type)
{
    return type < EFI_MEM_KERNEL ? efiMemTypeToString[type] : kernelMemTypeToString[type - EFI_MEM_KERNEL];
}

static void pmm_free_unlocked(void* address)
{
    freePageAmount++;

    if (firstPage == NULL)
    {
        firstPage = VMM_LOWER_TO_HIGHER(address);
        firstPage->next = NULL;
        lastPage = firstPage;
    }
    else
    {
        lastPage->next = VMM_LOWER_TO_HIGHER(address);
        lastPage->next->next = NULL;
        lastPage = lastPage->next;
    }
}

static void pmm_free_pages_unlocked(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        void* a = (void*)((uint64_t)address + i * PAGE_SIZE);
        pmm_free_unlocked(a);
    }
}

static void pmm_memory_map_deallocate(void)
{
    pmm_free_type(EFI_MEM_MEMORY_MAP);
    memoryMap.base = NULL;
}

#if CONFIG_PMM_LAZY
static void pmm_lazy_load_memory(void)
{
    static uint64_t index = 0;
    static bool full = false;

    if (full)
    {
        lock_release(&lock);
        debug_panic("PMM full");
    }

    while (true)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(&memoryMap, index);
        loadedPageAmount += desc->amountOfPages;
        index++;

        if (index >= memoryMap.descriptorAmount)
        {
            full = true;
            pmm_memory_map_deallocate();
            if (pmm_is_type_free(desc->type))
            {
                pmm_free_pages_unlocked(desc->physicalStart, desc->amountOfPages);
            }
            return;
        }

        if (pmm_is_type_free(desc->type))
        {
            pmm_free_pages_unlocked(desc->physicalStart, desc->amountOfPages);
            return;
        }
    }
}
#else
static void pmm_load_memory(void)
{
    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(&memoryMap, i);

        if (pmm_is_type_free(desc->type))
        {
            pmm_free_pages_unlocked(desc->physicalStart, desc->amountOfPages);
        }
    }
    loadedPageAmount = pageAmount;

    pmm_memory_map_deallocate();
}
#endif

static void pmm_detect_memory(void)
{
    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(&memoryMap, i);

        pageAmount += desc->amountOfPages;
    }

    log_print("UEFI-provided memory map: ");
    log_print("Detected memory: %d KB", (pageAmount * 0x1000) / 1000);

    if (CONFIG_PMM_LAZY)
    {
        log_print("pmm: lazy");
    }
}

void pmm_init(efi_mem_map_t* efi_mem_map_t)
{
    memoryMap = *efi_mem_map_t;
    lock_init(&lock);

    pmm_detect_memory();

#if !(CONFIG_PMM_LAZY)
    pmm_load_memory();
#endif
}

void pmm_free_type(uint32_t type)
{
    for (uint64_t i = 0; i < memoryMap.descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(&memoryMap, i);

        if (desc->type == type)
        {
            log_print("pmm: free [mem %a-%a] %s", desc->physicalStart, ((uintptr_t)desc->physicalStart) + desc->amountOfPages, pmm_mem_type_to_string(desc->type));
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
        }
    }
}

void* pmm_alloc(void)
{
    LOCK_GUARD(&lock);

    if (firstPage == NULL)
    {
#if CONFIG_PMM_LAZY
        pmm_lazy_load_memory();
#else
        debug_panic("Physical Memory Manager full!");
#endif
    }

    void* address = firstPage;
    firstPage = firstPage->next;
    freePageAmount--;

    return VMM_HIGHER_TO_LOWER(address);
}

void pmm_free(void* address)
{
    LOCK_GUARD(&lock);
    pmm_free_unlocked(address);
}

void pmm_free_pages(void* address, uint64_t count)
{
    LOCK_GUARD(&lock);
    pmm_free_pages_unlocked(address, count);
}

uint64_t pmm_total_amount(void)
{
    return pageAmount;
}

uint64_t pmm_free_amount(void)
{
    return freePageAmount + pageAmount - loadedPageAmount;
}

uint64_t pmm_reserved_amount(void)
{
    return pageAmount - pmm_free_amount();
}
