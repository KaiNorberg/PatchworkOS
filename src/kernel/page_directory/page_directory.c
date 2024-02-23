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

static inline PageDirectoryEntry page_directory_entry_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_FLAG_PRESENT);
}

static inline PageDirectory* page_directory_get_directory(PageDirectory* pageDirectory, uint64_t index)
{
    if (!PAGE_DIRECTORY_GET_FLAG(pageDirectory->entries[index], PAGE_FLAG_PRESENT))
    {
        return 0;
    }

    return vmm_physical_to_virtual(PAGE_DIRECTORY_GET_ADDRESS(pageDirectory->entries[index]));
}

static inline PageDirectoryEntry page_directory_get_or_create_entry(PageDirectory* pageDirectory, uint64_t index, uint64_t flags)
{
    if (!PAGE_DIRECTORY_GET_FLAG(pageDirectory->entries[index], PAGE_FLAG_PRESENT))
    {
        void* page = pmm_allocate();
        memset(vmm_physical_to_virtual(page), 0, PAGE_SIZE);
        pageDirectory->entries[index] = page_directory_entry_create(page, flags);
    }

    return pageDirectory->entries[index];
}

static inline PageDirectory* page_directory_get_or_create_directory(PageDirectory* pageDirectory, uint64_t index, uint64_t flags)
{
    PageDirectoryEntry entry = page_directory_get_or_create_entry(pageDirectory, index, flags);

    return vmm_physical_to_virtual(PAGE_DIRECTORY_GET_ADDRESS(entry));
}

PageDirectory* page_directory_new()
{
    PageDirectory* pageDirectory = (PageDirectory*)pmm_allocate();
    memset(vmm_physical_to_virtual(pageDirectory), 0, PAGE_SIZE);
    
    return pageDirectory;
}

void page_directory_copy_range(PageDirectory* dest, PageDirectory* src, uint64_t lowerIndex, uint64_t upperIndex)
{    
    dest = vmm_physical_to_virtual(dest);
    src = vmm_physical_to_virtual(src);

    for (uint64_t i = lowerIndex; i < upperIndex; i++)
    {
        dest->entries[i] = src->entries[i];
    }
}

void page_directory_populate_range(PageDirectory* pageDirectory, uint64_t lowerIndex, uint64_t upperIndex, uint64_t flags)
{    
    pageDirectory = vmm_physical_to_virtual(pageDirectory);

    for (uint64_t i = lowerIndex; i < upperIndex; i++)
    {
        page_directory_get_or_create_entry(pageDirectory, i, (flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR) & ~PAGE_FLAG_GLOBAL);
    }
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

    pageDirectory = vmm_physical_to_virtual(pageDirectory);

    uint64_t level4Index = PAGE_DIRECTORY_GET_INDEX(virtualAddress, 4);
    uint64_t level3Index = PAGE_DIRECTORY_GET_INDEX(virtualAddress, 3);
    uint64_t level2Index = PAGE_DIRECTORY_GET_INDEX(virtualAddress, 2);
    uint64_t level1Index = PAGE_DIRECTORY_GET_INDEX(virtualAddress, 1);

    PageDirectory* level3 = page_directory_get_or_create_directory(pageDirectory, level4Index, 
        (flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR) & ~PAGE_FLAG_GLOBAL);

    PageDirectory* level2 = page_directory_get_or_create_directory(level3, level3Index, 
        flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    PageDirectory* level1 = page_directory_get_or_create_directory(level2, level2Index, 
        flags | PAGE_FLAG_WRITE | PAGE_FLAG_USER_SUPERVISOR);

    level1->entries[level1Index] = page_directory_entry_create(physicalAddress, flags);
}

void page_directory_free(PageDirectory* pageDirectory)
{    
    pageDirectory = vmm_physical_to_virtual(pageDirectory);

    for (uint64_t level4Index = 0; level4Index < PAGE_DIRECTORY_ENTRY_AMOUNT; level4Index++)
    {    
        PageDirectory* level3 = page_directory_get_directory(pageDirectory, level4Index);
        if (level3 == 0)
        {   
            continue;
        }
        
        for (uint64_t level3Index = 0; level3Index < PAGE_DIRECTORY_ENTRY_AMOUNT; level3Index++)
        {                
            PageDirectory* level2 = page_directory_get_directory(level3, level3Index);
            if (level2 == 0)
            {
                continue;
            }

            for (uint64_t level2Index = 0; level2Index < PAGE_DIRECTORY_ENTRY_AMOUNT; level2Index++)
            {
                PageDirectory* level1 = page_directory_get_directory(level2, level2Index);
                if (level1 == 0)
                {
                    continue;
                }

                if (!PAGE_DIRECTORY_GET_FLAG(level2->entries[level2Index], PAGE_FLAG_DONT_FREE))
                {
                    pmm_free_page(vmm_virtual_to_physical(level1));
                }
            }
            if (!PAGE_DIRECTORY_GET_FLAG(level3->entries[level3Index], PAGE_FLAG_DONT_FREE))
            {
                pmm_free_page(vmm_virtual_to_physical(level2));
            }
        }
        if (!PAGE_DIRECTORY_GET_FLAG(pageDirectory->entries[level4Index], PAGE_FLAG_DONT_FREE))
        {
            pmm_free_page(vmm_virtual_to_physical(level3));
        }
    }
    pmm_free_page(vmm_virtual_to_physical(pageDirectory));            
}