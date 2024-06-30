#include "pmm.h"

#include <stddef.h>
#include <string.h>

#include <bootloader/boot_info.h>

#include "config.h"
#include "lock.h"
#include "log.h"
#include "vmm.h"

static const char* efiMemTypeToString[] = {
    "reserved memory type",
    "loader code",
    "loader data",
    "boot services code",
    "boot services data",
    "runtime services code",
    "runtime services data",
    "conventional memory",
    "unusable memory",
    "acpi reclaim memory",
    "acpi memory nvs",
    "memory mapped io",
    "memory mapped io port space",
    "pal code",
    "persistent memory",
};

static page_header_t* firstPage = NULL;
static page_header_t* lastPage = NULL;
static uint64_t pageAmount = 0;
static uint64_t loadedPageAmount = 0;
static uint64_t freePageAmount = 0;

static lock_t lock;

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

static void pmm_load_memory(efi_mem_map_t* memoryMap)
{
    log_print("pmm: load");

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        loadedPageAmount = pageAmount;

        if (EFI_IS_MEMORY_AVAIL(desc->type))
        {
            pmm_free_pages_unlocked(desc->physicalStart, desc->amountOfPages);
        }
    }
}

static void pmm_detect_memory(efi_mem_map_t* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        pageAmount += desc->amountOfPages;
    }

    log_print("UEFI-provided memory map: ");
    log_print("Detected memory: %d KB", (pageAmount * PAGE_SIZE) / 1024);
}

void pmm_init(efi_mem_map_t* memoryMap)
{
    lock_init(&lock);

    pmm_detect_memory(memoryMap);
    pmm_load_memory(memoryMap);
}

void* pmm_alloc(void)
{
    LOCK_GUARD(&lock);

    if (firstPage == NULL)
    {
        log_panic(NULL, "Physical Memory Manager full");
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
