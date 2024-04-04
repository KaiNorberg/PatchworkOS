#include "pmm.h"

#include <stddef.h>
#include <string.h>

#include <common/boot_info/boot_info.h>

#include "debug/debug.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "utils/utils.h"

static PageHeader* firstPage = NULL;
static PageHeader* lastPage = NULL;
static uint64_t pageAmount = 0;
static uint64_t loadedPageAmount = 0;
static uint64_t freePageAmount = 0;

static EfiMemoryMap* memoryMap;

static Lock lock;

static uint8_t is_type_usable(uint64_t memoryType)
{
	return memoryType == EFI_CONVENTIONAL_MEMORY ||
		memoryType == EFI_LOADER_CODE ||
		memoryType == EFI_LOADER_DATA ||
		memoryType == EFI_BOOT_SERVICES_CODE ||
		memoryType == EFI_BOOT_SERVICES_DATA;
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

#if CONFIG_PMM_LAZY
static void pmm_lazy_load_memory()
{
    static uint64_t i = 0;
    for (; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        loadedPageAmount += desc->amountOfPages;

        if (is_type_usable(desc->type))
        {
            pmm_free_pages_unlocked(desc->physicalStart, desc->amountOfPages);

            i++;
            return;
        }
    }

    lock_release(&lock);
    debug_panic("Physical Memory Manager full!");
}
#else
static void pmm_load_memory()
{    
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        loadedPageAmount += desc->amountOfPages;

        if (is_type_usable(desc->type))
        {
            pmm_free_pages_unlocked(desc->physicalStart, desc->amountOfPages);
        }
    }
}
#endif

static void pmm_detect_memory()
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        pageAmount += desc->amountOfPages;
    }
}

void pmm_init(EfiMemoryMap* efiMemoryMap)
{   
    memoryMap = efiMemoryMap;
    lock = lock_create();

    pmm_detect_memory();

#if !(CONFIG_PMM_LAZY)
    pmm_load_memory();
#endif
}

void* pmm_allocate(void)
{
    LOCK_GUARD(lock);

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
    LOCK_GUARD(lock);
    pmm_free_unlocked(address);
}

void pmm_free_pages(void* address, uint64_t count)
{
    LOCK_GUARD(lock);
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