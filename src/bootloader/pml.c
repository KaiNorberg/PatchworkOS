#include "pml.h"

#include <common/boot_info.h>

#include "mem.h"

static pml_entry_t pml_entry_create(void* address, uint64_t flags)
{
    return ((((uintptr_t)address >> 12) & 0x000000FFFFFFFFFF) << 12) | (flags | (uint64_t)PAGE_PRESENT);
}

pml_t* pml_new(void)
{
    pml_t* pageTable = (pml_t*)mem_alloc_pages(1, EFI_PML_MEMORY);
    SetMem(pageTable, EFI_PAGE_SIZE, 0);

    return pageTable;
}

void pml_map_pages(pml_t* pageTable, void* virtAddr, void* physAddr, uint64_t pageAmount, uint16_t flags)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        pml_map(pageTable, (void*)((uint64_t)virtAddr + page * EFI_PAGE_SIZE), (void*)((uint64_t)physAddr + page * EFI_PAGE_SIZE),
            flags);
    }
}

void pml_map(pml_t* pageTable, void* virtAddr, void* physAddr, uint16_t flags)
{
    if ((uint64_t)virtAddr % EFI_PAGE_SIZE != 0)
    {
        Print(L"ERROR: Attempt to map invalid virtual address!");
    }
    else if ((uint64_t)physAddr % EFI_PAGE_SIZE != 0)
    {
        Print(L"ERROR: Attempt to map invalid physical address!");
    }

    uint64_t indexer = (uint64_t)virtAddr;
    indexer >>= 12;
    uint64_t pIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t ptIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdpIndex = indexer & 0x1ff;

    pml_entry_t entry = pageTable->entries[pdpIndex];
    pml_t* pdp;
    if ((entry & PAGE_PRESENT) == 0)
    {
        pdp = (pml_t*)mem_alloc_pages(1, EFI_PML_MEMORY);
        SetMem(pdp, EFI_PAGE_SIZE, 0);

        entry = pml_entry_create(pdp, flags);
        pageTable->entries[pdpIndex] = entry;
    }
    else
    {
        pdp = (pml_t*)(PML_GET_ADDRESS(entry));
    }

    entry = pdp->entries[pdIndex];
    pml_t* pd;
    if ((entry & PAGE_PRESENT) == 0)
    {
        pd = (pml_t*)mem_alloc_pages(1, EFI_PML_MEMORY);
        SetMem(pd, EFI_PAGE_SIZE, 0);

        entry = pml_entry_create(pd, flags);
        pdp->entries[pdIndex] = entry;
    }
    else
    {
        pd = (pml_t*)(PML_GET_ADDRESS(entry));
    }

    entry = pd->entries[ptIndex];
    pml_t* pt;
    if ((entry & PAGE_PRESENT) == 0)
    {
        pt = (pml_t*)mem_alloc_pages(1, EFI_PML_MEMORY);
        SetMem(pt, EFI_PAGE_SIZE, 0);

        entry = pml_entry_create(pt, flags);
        pd->entries[ptIndex] = entry;
    }
    else
    {

        pt = (pml_t*)(PML_GET_ADDRESS(entry));
    }

    entry = pml_entry_create(physAddr, flags);
    pt->entries[pIndex] = entry;
}
