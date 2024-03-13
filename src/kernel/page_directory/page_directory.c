#include "page_directory.h"

#include <libc/string.h>

#include "pmm/pmm.h"
#include "debug/debug.h"
#include "vmm/vmm.h"
#include "registers/registers.h"

static inline Pde pde_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_FLAG_PRESENT);
}

static inline PageDirectory* page_directory_get(PageDirectory* pageDirectory, uint64_t index)
{
    Pde entry = pageDirectory->entries[index];

    if (!PDE_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        return 0;
    }

    return vmm_physical_to_virtual(PDE_GET_ADDRESS(entry));
}

static inline PageDirectory* page_directory_get_or_allocate(PageDirectory* pageDirectory, uint64_t index, uint64_t flags)
{
    Pde entry = pageDirectory->entries[index];

    if (PDE_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        return vmm_physical_to_virtual(PDE_GET_ADDRESS(entry));
    }
    else
    {
        PageDirectory* address = vmm_physical_to_virtual(pmm_allocate());
        memset(address, 0, PAGE_SIZE);

        pageDirectory->entries[index] = pde_create(vmm_virtual_to_physical(address), flags);

        return address;
    }
}

static inline void page_directory_free_level(PageDirectory* pageDirectory, int64_t level)
{
    if (level < 0)
    {
        return;
    }

    for (uint64_t i = 0; i < PDE_AMOUNT; i++)
    {   
        Pde entry = pageDirectory->entries[i];
        if (!PDE_GET_FLAG(entry, PAGE_FLAG_PRESENT))
        {
            continue;
        }

        if (!PDE_GET_FLAG(entry, PAGE_FLAG_KERNEL))
        {
            page_directory_free_level(vmm_physical_to_virtual(PDE_GET_ADDRESS(entry)), level - 1);
        }
    }
    pmm_free_page(vmm_virtual_to_physical(pageDirectory));
}

PageDirectory* page_directory_new(void)
{
    PageDirectory* pageDirectory = vmm_physical_to_virtual(pmm_allocate());
    memset(pageDirectory, 0, PAGE_SIZE);
    
    return pageDirectory;
}

void page_directory_free(PageDirectory* pageDirectory)
{    
    page_directory_free_level(pageDirectory, 4);
}

void page_directory_load(PageDirectory* pageDirectory)
{
    cr3_write((uint64_t)vmm_virtual_to_physical(pageDirectory));
}

void page_directory_map_pages(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        page_directory_map(pageDirectory, (void*)((uint64_t)virtualAddress + page * PAGE_SIZE), (void*)((uint64_t)physicalAddress + page * PAGE_SIZE), flags);
    }
}

void page_directory_map(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint16_t flags)
{        
    if ((uint64_t)virtualAddress % PAGE_SIZE != 0)
    {
        debug_panic("Attempt to map invalid virtual address!");
    }    
    else if ((uint64_t)physicalAddress % PAGE_SIZE != 0)
    {
        debug_panic("Attempt to map invalid physical address!");
    }

    PageDirectory* level3 = page_directory_get_or_allocate(pageDirectory, PAGE_DIRECTORY_GET_INDEX(virtualAddress, 4), 
        (flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR) & ~PAGE_FLAG_GLOBAL);

    PageDirectory* level2 = page_directory_get_or_allocate(level3, PAGE_DIRECTORY_GET_INDEX(virtualAddress, 3), 
        flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    PageDirectory* level1 = page_directory_get_or_allocate(level2, PAGE_DIRECTORY_GET_INDEX(virtualAddress, 2), 
        flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    Pde* entry = &level1->entries[PAGE_DIRECTORY_GET_INDEX(virtualAddress, 1)];

    if (PDE_GET_FLAG(*entry, PAGE_FLAG_PRESENT))
    {
        debug_panic("Attempted to map already mapped page");
    }

    *entry = pde_create(physicalAddress, flags);
}

void page_directory_change_flags(PageDirectory* pageDirectory, void* virtualAddress, uint16_t flags)
{
    if ((uint64_t)virtualAddress % PAGE_SIZE != 0)
    {
        debug_panic("Attempt to map invalid virtual address!");
    }

    PageDirectory* level3 = page_directory_get(pageDirectory, PAGE_DIRECTORY_GET_INDEX(virtualAddress, 4));
    if (level3 == 0)
    {
        debug_panic("Failed to change page flags");
    }

    PageDirectory* level2 = page_directory_get(level3, PAGE_DIRECTORY_GET_INDEX(virtualAddress, 3));
    if (level2 == 0)
    {
        debug_panic("Failed to change page flags");
    }

    PageDirectory* level1 = page_directory_get(level2, PAGE_DIRECTORY_GET_INDEX(virtualAddress, 2));
    if (level1 == 0)
    {
        debug_panic("Failed to change page flags");
    }

    Pde* entry = &level1->entries[PAGE_DIRECTORY_GET_INDEX(virtualAddress, 1)];

    if (!PDE_GET_FLAG(*entry, PAGE_FLAG_PRESENT))
    {
        debug_panic("Failed to change page flags");
    }

    *entry = pde_create(PDE_GET_ADDRESS(*entry), flags);
}