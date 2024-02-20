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

/*static inline PageDirectoryEntry page_directory_get_entry(PageDirectory const* pageDirectory, uint64_t index)
{
    if (!PAGE_DIRECTORY_GET_FLAG(pageDirectory->entries[index], PAGE_FLAG_PRESENT))
    {
        debug_panic("Page Directory entry does not exist!");
    }
    return pageDirectory->entries[index];
}*/

static inline PageDirectoryEntry page_directory_get_or_allocate_entry(PageDirectory* pageDirectory, uint64_t index, uint64_t flags)
{
    if (!PAGE_DIRECTORY_GET_FLAG(pageDirectory->entries[index], PAGE_FLAG_PRESENT))
    {
        void* page = pmm_request();
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
    PageDirectory* pageDirectory = (PageDirectory*)pmm_request();
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

    PageDirectory* level3 = page_directory_get_or_allocate_directory(pageDirectory, level4Index, PAGE_FLAG_READ_WRITE | PAGE_FLAG_USER_SUPERVISOR);
    PageDirectory* level2 = page_directory_get_or_allocate_directory(level3, level3Index, PAGE_FLAG_READ_WRITE | PAGE_FLAG_USER_SUPERVISOR);
    PageDirectory* level1 = page_directory_get_or_allocate_directory(level2, level2Index, PAGE_FLAG_READ_WRITE | PAGE_FLAG_USER_SUPERVISOR);
    level1->entries[level1Index] = PAGE_DIRECTORY_ENTRY_CREATE(physicalAddress, flags);
}

void* page_directory_get_physical_address(PageDirectory const* pageDirectory, void* virtualAddress)
{
    /*uint64_t indexer = round_down((uint64_t)virtualAddress, 0x1000);
    uint64_t offset = (uint64_t)virtualAddress - indexer;
    indexer >>= 12;
    uint64_t pIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t ptIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdpIndex = indexer & 0x1ff;

    if (!PAGE_DIRECTORY_GET_FLAG(pageDirectory->entries[pdpIndex], PAGE_FLAG_PRESENT))
    {
        return 0;
    }
    PageDirectory const* pdp = (PageDirectory*)PAGE_DIRECTORY_GET_ADDRESS(pageDirectory->entries[pdpIndex]);

    if (!PAGE_DIRECTORY_GET_FLAG(pdp->entries[pdIndex], PAGE_FLAG_PRESENT))
    {
        return 0;
    }
    PageDirectory const* pd = (PageDirectory*)PAGE_DIRECTORY_GET_ADDRESS(pdp->entries[pdIndex]);

    if (!PAGE_DIRECTORY_GET_FLAG(pd->entries[ptIndex], PAGE_FLAG_PRESENT))
    {
        return 0;
    }
    PageDirectory const* pt = (PageDirectory*)PAGE_DIRECTORY_GET_ADDRESS(pd->entries[ptIndex]);

    return (void*)((uint64_t)PAGE_DIRECTORY_GET_ADDRESS(pt->entries[pIndex]) + offset);*/

    return 0;
}

void page_directory_free(PageDirectory* pageDirectory)
{    
    /*PageDirectoryEntry entry;

    for (uint64_t pdpIndex = 0; pdpIndex < 512; pdpIndex++)
    {
        entry = pageDirectory->entries[pdpIndex];
        PageDirectory* pdp;
        if (PAGE_DIRECTORY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
        {
            pdp = (PageDirectory*)(PAGE_DIRECTORY_GET_ADDRESS(entry));        
            for (uint64_t pdIndex = 0; pdIndex < 512; pdIndex++)
            {
                entry = pdp->entries[pdIndex]; 
                PageDirectory* pd;
                if (PAGE_DIRECTORY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
                {
                    pd = (PageDirectory*)(PAGE_DIRECTORY_GET_ADDRESS(entry));
                    for (uint64_t ptIndex = 0; ptIndex < 512; ptIndex++)
                    {
                        entry = pd->entries[ptIndex];
                        PageDirectory* pt;
                        if (PAGE_DIRECTORY_GET_FLAG(entry, PAGE_FLAG_PRESENT))
                        {
                            pt = (PageDirectory*)(PAGE_DIRECTORY_GET_ADDRESS(entry));
                            pmm_unlock_page(pt);
                        }
                    }
                    pmm_unlock_page(pd);
                }
            }
            pmm_unlock_page(pdp);
        }
    }

    pmm_unlock_page(pageDirectory);*/
}