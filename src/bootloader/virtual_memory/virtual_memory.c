#include "virtual_memory.h"

#include "string/string.h"
#include "memory/memory.h"

#include "../common.h"

VirtualAddressSpace* virtual_memory_create()
{
    VirtualAddressSpace* addressSpace = (VirtualAddressSpace*)memory_allocate_pages(1, EFI_PAGE_TABLE_MEMORY_TYPE);
    memset(addressSpace, 0, 0x1000);

    return addressSpace;
}

void virtual_memory_remap(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress)
{        
    uint64_t indexer = (uint64_t)virtualAddress;
    indexer >>= 12;
    uint64_t pIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t ptIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdIndex = indexer & 0x1ff;
    indexer >>= 9;
    uint64_t pdpIndex = indexer & 0x1ff;

    PageDirEntry pde = addressSpace->entries[pdpIndex];
    PageDirectory* pdp;
    if (!PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
    {
        pdp = (PageDirectory*)memory_allocate_pages(1, EFI_PAGE_TABLE_MEMORY_TYPE);
        memset(pdp, 0, 0x1000);
        PAGE_DIR_SET_ADDRESS(pde, (uint64_t)pdp >> 12);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_PRESENT);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_READ_WRITE);

        addressSpace->entries[pdpIndex] = pde;
    }
    else
    {        
        pdp = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);
    }
    
    pde = pdp->entries[pdIndex];
    PageDirectory* pd;
    if (!PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
    {
        pd = (PageDirectory*)memory_allocate_pages(1, EFI_PAGE_TABLE_MEMORY_TYPE);
        memset(pd, 0, 0x1000);
        PAGE_DIR_SET_ADDRESS(pde, (uint64_t)pd >> 12);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_PRESENT);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_READ_WRITE);

        pdp->entries[pdIndex] = pde;
    }
    else
    {          
        pd = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);
    }

    pde = pd->entries[ptIndex];
    PageDirectory* pt;
    if (!PAGE_DIR_GET_FLAG(pde, PAGE_DIR_PRESENT))
    {
        pt = (PageDirectory*)memory_allocate_pages(1, EFI_PAGE_TABLE_MEMORY_TYPE);
        memset(pt, 0, 0x1000);
        PAGE_DIR_SET_ADDRESS(pde, (uint64_t)pt >> 12);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_PRESENT);
        PAGE_DIR_SET_FLAG(pde, PAGE_DIR_READ_WRITE);

        pd->entries[ptIndex] = pde;
    }
    else
    {   
        pt = (PageDirectory*)((uint64_t)PAGE_DIR_GET_ADDRESS(pde) << 12);
    }

    pde = pt->entries[pIndex];
    PAGE_DIR_SET_ADDRESS(pde, (uint64_t)physicalAddress >> 12);
    PAGE_DIR_SET_FLAG(pde, PAGE_DIR_PRESENT);
    PAGE_DIR_SET_FLAG(pde, PAGE_DIR_READ_WRITE);

    pt->entries[pIndex] = pde;
}

void virtual_memory_remap_pages(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress, uint64_t pageAmount)
{
    for (uint64_t page = 0; page < pageAmount; page++)
    {
        virtual_memory_remap(addressSpace, (void*)((uint64_t)virtualAddress + page * 0x1000), (void*)((uint64_t)physicalAddress + page * 0x1000));
    }
}
