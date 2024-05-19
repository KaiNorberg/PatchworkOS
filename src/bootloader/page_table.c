#include "page_table.h"

#include <common/boot_info.h>

#include "memory.h"

static PageEntry page_table_entry_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_FLAG_PRESENT);
}

PageTable* page_table_new(void)
{
    PageTable* pageTable = (PageTable*)memory_allocate_pages(1, EFI_MEMORY_TYPE_PAGE_TABLE);
    SetMem(pageTable, EFI_PAGE_SIZE, 0);

    return pageTable;
}

void page_table_map_pages(PageTable* pageTable, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        page_table_map(pageTable, (void*)((uint64_t)virtualAddress + page * EFI_PAGE_SIZE), (void*)((uint64_t)physicalAddress + page * EFI_PAGE_SIZE), flags);
    }
}

void page_table_map(PageTable* pageTable, void* virtualAddress, void* physicalAddress, uint16_t flags)
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

    PageEntry entry = pageTable->entries[pdpIndex];
    PageTable* pdp;
    if (!PAGE_TABLE_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        pdp = (PageTable*)memory_allocate_pages(1, EFI_MEMORY_TYPE_PAGE_TABLE);
        SetMem(pdp, EFI_PAGE_SIZE, 0);

        entry = page_table_entry_create(pdp, flags);
        pageTable->entries[pdpIndex] = entry;
    }
    else
    {        
        pdp = (PageTable*)(PAGE_TABLE_GET_ADDRESS(entry));
    }
    
    entry = pdp->entries[pdIndex];
    PageTable* pd;
    if (!PAGE_TABLE_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        pd = (PageTable*)memory_allocate_pages(1, EFI_MEMORY_TYPE_PAGE_TABLE);
        SetMem(pd, EFI_PAGE_SIZE, 0);

        entry = page_table_entry_create(pd, flags);
        pdp->entries[pdIndex] = entry;
    }
    else
    {          
        pd = (PageTable*)(PAGE_TABLE_GET_ADDRESS(entry));
    }

    entry = pd->entries[ptIndex];
    PageTable* pt;
    if (!PAGE_TABLE_GET_FLAG(entry, PAGE_FLAG_PRESENT))
    {
        pt = (PageTable*)memory_allocate_pages(1, EFI_MEMORY_TYPE_PAGE_TABLE);
        SetMem(pt, EFI_PAGE_SIZE, 0);

        entry = page_table_entry_create(pt, flags);
        pd->entries[ptIndex] = entry;
    }
    else
    {   

        pt = (PageTable*)(PAGE_TABLE_GET_ADDRESS(entry));
    }

    entry = page_table_entry_create(physicalAddress, flags);
    pt->entries[pIndex] = entry;
}