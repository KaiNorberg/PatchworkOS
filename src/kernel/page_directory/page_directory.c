#include "page_directory.h"

#include "page_allocator/page_allocator.h"

#include "string/string.h"
#include "debug/debug.h"
#include "tty/tty.h"
#include "gdt/gdt.h"
#include "idt/idt.h"
#include "utils/utils.h"
#include "global_heap/global_heap.h"
#include "interrupts/interrupts.h"

#include "../common.h"

PageDirectory* kernelPageDirectory;

void page_directory_init(EfiMemoryMap* memoryMap, Framebuffer* screenbuffer)
{    
    tty_start_message("Page directory initializing");    
      
    kernelPageDirectory = (PageDirectory*)page_allocator_request();
    memset(kernelPageDirectory, 0, 0x1000);

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        
        page_directory_remap_pages(kernelPageDirectory, desc->virtualStart, desc->physicalStart, desc->amountOfPages, PAGE_DIR_READ_WRITE);
	}
    page_directory_remap_pages(kernelPageDirectory, screenbuffer->base, screenbuffer->base, GET_SIZE_IN_PAGES(screenbuffer->size), PAGE_DIR_READ_WRITE);
    PAGE_DIRECTORY_LOAD_SPACE(kernelPageDirectory);

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));

		if (desc->type == EFI_PAGE_TABLE_MEMORY_TYPE)
		{
            page_allocator_unlock_pages(desc->physicalStart, desc->amountOfPages);
		}
	}    
    tty_end_message(TTY_MESSAGE_OK);
}

PageDirectory* page_directory_new()
{
    PageDirectory* pageDirectory = (PageDirectory*)page_allocator_request();
    memset(pageDirectory, 0, 0x1000);

    global_heap_map(pageDirectory);
    interrupt_vectors_map(pageDirectory);
    
    return pageDirectory;
}

void page_directory_remap_pages(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        page_directory_remap(pageDirectory, (void*)((uint64_t)virtualAddress + page * 0x1000), (void*)((uint64_t)physicalAddress + page * 0x1000), flags);
    }
}

void page_directory_remap(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint16_t flags)
{    
    //Im pretty confident something here is wrong
    
    if ((uint64_t)virtualAddress % 0x1000 != 0)
    {
        debug_panic("Attempt to map invalid virtual address!");
    }    
    else if ((uint64_t)physicalAddress % 0x1000 != 0)
    {
        debug_panic("Attempt to map invalid physical address!");
    }

    uint64_t indexer = (uint64_t)virtualAddress;
    indexer >>= 12;
    uint64_t pIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t ptIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdpIndex = indexer & 0x1ff;

    if (!PAGE_DIR_GET_FLAG(pageDirectory->entries[pdpIndex], PAGE_DIR_PRESENT))
    {
        void* page = (PageDirectory*)page_allocator_request();
        memset(page, 0, 0x1000);

        pageDirectory->entries[pdpIndex] = PAGE_DIR_ENTRY_CREATE(page, flags);
    }
    PageDirectory* pdp = (PageDirectory*)PAGE_DIR_GET_ADDRESS(pageDirectory->entries[pdpIndex]);

    if (!PAGE_DIR_GET_FLAG(pdp->entries[pdIndex], PAGE_DIR_PRESENT))
    {
        void* page = (PageDirectory*)page_allocator_request();
        memset(page, 0, 0x1000);

        pdp->entries[pdIndex] = PAGE_DIR_ENTRY_CREATE(page, flags);
    }
    PageDirectory* pd = (PageDirectory*)PAGE_DIR_GET_ADDRESS(pdp->entries[pdIndex]);

    if (!PAGE_DIR_GET_FLAG(pd->entries[ptIndex], PAGE_DIR_PRESENT))
    {
        void* page = (PageDirectory*)page_allocator_request();
        memset(page, 0, 0x1000);

        pd->entries[ptIndex] = PAGE_DIR_ENTRY_CREATE(page, flags);
    }
    PageDirectory* pt = (PageDirectory*)PAGE_DIR_GET_ADDRESS(pd->entries[ptIndex]);

    if (PAGE_DIR_GET_FLAG(pt->entries[pIndex], PAGE_DIR_PRESENT))
    {
        page_directory_invalidate_page(virtualAddress);
    }
    pt->entries[pIndex] = PAGE_DIR_ENTRY_CREATE(physicalAddress, flags);
}

void* page_directory_get_physical_address(PageDirectory* pageDirectory, void* virtualAddress)
{
    uint64_t indexer = round_down((uint64_t)virtualAddress, 0x1000);
    uint64_t offset = (uint64_t)virtualAddress - indexer;
    indexer >>= 12;
    uint64_t pIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t ptIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdpIndex = indexer & 0x1ff;

    if (!PAGE_DIR_GET_FLAG(pageDirectory->entries[pdpIndex], PAGE_DIR_PRESENT))
    {
        return 0;
    }
    PageDirectory* pdp = (PageDirectory*)PAGE_DIR_GET_ADDRESS(pageDirectory->entries[pdpIndex]);

    if (!PAGE_DIR_GET_FLAG(pdp->entries[pdIndex], PAGE_DIR_PRESENT))
    {
        return 0;
    }
    PageDirectory* pd = (PageDirectory*)PAGE_DIR_GET_ADDRESS(pdp->entries[pdIndex]);

    if (!PAGE_DIR_GET_FLAG(pd->entries[ptIndex], PAGE_DIR_PRESENT))
    {
        return 0;
    }
    PageDirectory* pt = (PageDirectory*)PAGE_DIR_GET_ADDRESS(pd->entries[ptIndex]);

    uint64_t physicalAddress = PAGE_DIR_GET_ADDRESS(pt->entries[pIndex]);
    return (void*)(physicalAddress + offset);
}

void page_directory_free(PageDirectory* pageDirectory)
{    
    PageDirectoryEntry pde;

    for (uint64_t pdpIndex = 0; pdpIndex < 512; pdpIndex++)
    {
        pde = pageDirectory->entries[pdpIndex];
        PageDirectory* pdp;
        if (PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
        {
            pdp = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde));        
            for (uint64_t pdIndex = 0; pdIndex < 512; pdIndex++)
            {
                pde = pdp->entries[pdIndex]; 
                PageDirectory* pd;
                if (PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
                {
                    pd = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde));
                    for (uint64_t ptIndex = 0; ptIndex < 512; ptIndex++)
                    {
                        pde = pd->entries[ptIndex];
                        PageDirectory* pt;
                        if (PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
                        {
                            pt = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde));
                            page_allocator_unlock_page(pt);
                        }
                    }
                    page_allocator_unlock_page(pd);
                }
            }
            page_allocator_unlock_page(pdp);
        }
    }

    page_allocator_unlock_page(pageDirectory);
}