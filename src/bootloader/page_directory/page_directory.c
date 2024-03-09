#include "page_directory.h"

#include "memory/memory.h"
#include "string/string.h"
#include <common/boot_info/boot_info.h>
#include "efilib.h"

static inline Pde page_directory_entry_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_FLAG_PRESENT);
}

PageDirectory* page_directory_new()
{
    PageDirectory* pageDirectory = (PageDirectory*)memory_allocate_pages(1, EFI_MEMORY_TYPE_PAGE_DIRECTORY);
    SetMem(pageDirectory, EFI_PAGE_SIZE, 0);

    return pageDirectory;
}

void page_directory_map_pages(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        page_directory_map(pageDirectory, (void*)((uint64_t)virtualAddress + page * EFI_PAGE_SIZE), (void*)((uint64_t)physicalAddress + page * EFI_PAGE_SIZE), flags);
    }
}

void page_directory_map(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint16_t flags)
{        
    if ((uint64_t)virtualAddress % EFI_PAGE_SIZE != 0)
    {
        Print(L"ERROR: Attempt to map invalid virtual address!");
    }    
    else if ((uint64_t)physicalAddress % EFI_PAGE_SIZE != 0)
    {
        Print(L"ERROR: Attempt to map invalid physical address!");
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

    Pde pde = pageDirectory->entries[pdpIndex];
    PageDirectory* pdp;
    if (!PAGE_DIRECTORY_GET_FLAG(pde, PAGE_FLAG_PRESENT))
    {
        pdp = (PageDirectory*)memory_allocate_pages(1, EFI_MEMORY_TYPE_PAGE_DIRECTORY);
        SetMem(pdp, EFI_PAGE_SIZE, 0);

        pde = page_directory_entry_create(pdp, flags);
        pageDirectory->entries[pdpIndex] = pde;
    }
    else
    {        
        pdp = (PageDirectory*)(PAGE_DIRECTORY_GET_ADDRESS(pde));
    }
    
    pde = pdp->entries[pdIndex];
    PageDirectory* pd;
    if (!PAGE_DIRECTORY_GET_FLAG(pde, PAGE_FLAG_PRESENT))
    {
        pd = (PageDirectory*)memory_allocate_pages(1, EFI_MEMORY_TYPE_PAGE_DIRECTORY);
        SetMem(pd, EFI_PAGE_SIZE, 0);

        pde = page_directory_entry_create(pd, flags);
        pdp->entries[pdIndex] = pde;
    }
    else
    {          
        pd = (PageDirectory*)(PAGE_DIRECTORY_GET_ADDRESS(pde));
    }

    pde = pd->entries[ptIndex];
    PageDirectory* pt;
    if (!PAGE_DIRECTORY_GET_FLAG(pde, PAGE_FLAG_PRESENT))
    {
        pt = (PageDirectory*)memory_allocate_pages(1, EFI_MEMORY_TYPE_PAGE_DIRECTORY);
        SetMem(pt, EFI_PAGE_SIZE, 0);

        pde = page_directory_entry_create(pt, flags);
        pd->entries[ptIndex] = pde;
    }
    else
    {   

        pt = (PageDirectory*)(PAGE_DIRECTORY_GET_ADDRESS(pde));
    }

    pde = page_directory_entry_create(physicalAddress, flags);
    pt->entries[pIndex] = pde;
}