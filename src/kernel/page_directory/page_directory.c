#include "page_directory.h"

#include "pmm/pmm.h"
#include "debug/debug.h"
#include "tty/tty.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "vmm/vmm.h"

#include "worker/interrupts/interrupts.h"
#include "worker/program_loader/program_loader.h"

#include <libc/string.h>
#include <common/common.h>

static inline PageDirectoryEntry page_directory_get_entry(PageDirectory const* pageDirectory, uint64_t index)
{
    if (!PAGE_DIRECTORY_GET_FLAG(pageDirectory->entries[index], PAGE_FLAG_PRESENT))
    {
        return 0;
    }
    return pageDirectory->entries[index];
}

static inline PageDirectory* page_directory_get_directory(PageDirectory* pageDirectory, uint64_t index)
{
    PageDirectoryEntry entry = page_directory_get_entry(pageDirectory, index);

    if (entry == 0)
    {
        return 0;
    }

    return vmm_physical_to_virtual(PAGE_DIRECTORY_GET_ADDRESS(entry));
}

static inline PageDirectoryEntry page_directory_get_or_allocate_entry(PageDirectory* pageDirectory, uint64_t index, uint64_t flags)
{
    if (!PAGE_DIRECTORY_GET_FLAG(pageDirectory->entries[index], PAGE_FLAG_PRESENT))
    {
        void* page = pmm_allocate();
        memset(vmm_physical_to_virtual(page), 0, 0x1000);
        pageDirectory->entries[index] = PAGE_DIRECTORY_ENTRY_CREATE(page, flags);
    }

    return pageDirectory->entries[index];
}

static inline PageDirectory* page_directory_get_or_allocate_directory(PageDirectory* pageDirectory, uint64_t index, uint64_t flags)
{
    PageDirectoryEntry entry = page_directory_get_or_allocate_entry(pageDirectory, index, flags);

    return vmm_physical_to_virtual(PAGE_DIRECTORY_GET_ADDRESS(entry));
}

PageDirectory* page_directory_new()
{
    PageDirectory* pageDirectory = (PageDirectory*)pmm_allocate();
    memset(vmm_physical_to_virtual(pageDirectory), 0, 0x1000);
    
    return pageDirectory;
}

void page_directory_map_pages(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        page_directory_map(pageDirectory, (void*)((uint64_t)virtualAddress + page * 0x1000), (void*)((uint64_t)physicalAddress + page * 0x1000), flags);
    }
}

void page_directory_map(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint16_t flags)
{        
    if ((uint64_t)virtualAddress % 0x1000 != 0)
    {
        debug_panic("Attempt to map invalid virtual address!");
    }    
    else if ((uint64_t)physicalAddress % 0x1000 != 0)
    {
        debug_panic("Attempt to map invalid physical address!");
    }

    pageDirectory = vmm_physical_to_virtual(pageDirectory);

    uint64_t level4Index = PAGE_DIRECTORY_GET_INDEX(virtualAddress, 4);
    uint64_t level3Index = PAGE_DIRECTORY_GET_INDEX(virtualAddress, 3);
    uint64_t level2Index = PAGE_DIRECTORY_GET_INDEX(virtualAddress, 2);
    uint64_t level1Index = PAGE_DIRECTORY_GET_INDEX(virtualAddress, 1);

    PageDirectory* level3 = page_directory_get_or_allocate_directory(pageDirectory, level4Index, PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);
    PageDirectory* level2 = page_directory_get_or_allocate_directory(level3, level3Index, PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);
    PageDirectory* level1 = page_directory_get_or_allocate_directory(level2, level2Index, PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);
    level1->entries[level1Index] = PAGE_DIRECTORY_ENTRY_CREATE(physicalAddress, flags);
}

void page_directory_free(PageDirectory* pageDirectory)
{    
    pageDirectory = vmm_physical_to_virtual(pageDirectory);

    for (uint64_t level4Index = 0; level4Index < 512; level4Index++)
    {
        PageDirectory* level3 = page_directory_get_directory(pageDirectory, level4Index);
        if (level3 == 0)
        {   
            continue;
        }
        
        for (uint64_t level3Index = 0; level3Index < 512; level3Index++)
        {
            PageDirectory* level2 = page_directory_get_directory(level3, level3Index);
            if (level2 == 0)
            {
                continue;
            }

            for (uint64_t level2Index = 0; level2Index < 512; level2Index++)
            {
                PageDirectory* level1 = page_directory_get_directory(level2, level2Index);
                if (level1 == 0)
                {
                    continue;
                }

                pmm_unlock_page(vmm_virtual_to_physical(level1));
            }
            pmm_unlock_page(vmm_virtual_to_physical(level2));
        }
        pmm_unlock_page(vmm_virtual_to_physical(level3));
    }
    pmm_unlock_page(vmm_virtual_to_physical(pageDirectory));
}