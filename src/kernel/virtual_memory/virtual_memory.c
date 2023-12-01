#include "virtual_memory.h"

#include "kernel/page_allocator/page_allocator.h"

#include "libc/include/string.h"

VirtualAddressSpace* currentAddressSpace;

VirtualAddressSpace* virtual_memory_create()
{
    VirtualAddressSpace* addressSpace = (VirtualAddressSpace*)page_allocator_request();
    memset(addressSpace, 0, 0x1000);

    return addressSpace;
}

void virtual_memory_load_space(VirtualAddressSpace* addressSpace)
{
    currentAddressSpace = addressSpace;
    asm volatile ("mov %0, %%cr3" : : "r" (addressSpace));
}

void virtual_memory_remap_current(void* virtualAddress, void* physicalAddress)
{
    virtual_memory_remap(currentAddressSpace, virtualAddress, physicalAddress);
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

    PageDirEntry pde;

    pde = addressSpace->Entries[pdpIndex];
    PageTable* pdp;
    if (!pde.Present)
    {
        pdp = (PageTable*)page_allocator_request();
        memset(pdp, 0, 0x1000);
        pde.Address = (uint64_t)pdp >> 12;
        pde.Present = 1;
        pde.ReadWrite = 1;
        addressSpace->Entries[pdpIndex] = pde;
    }
    else
    {
        pdp = (PageTable*)((uint64_t)pde.Address << 12);
    }
    
    
    pde = pdp->Entries[pdIndex];
    PageTable* pd;
    if (!pde.Present)
    {
        pd = (PageTable*)page_allocator_request();
        memset(pd, 0, 0x1000);
        pde.Address = (uint64_t)pd >> 12;
        pde.Present = 1;
        pde.ReadWrite = 1;
        pdp->Entries[pdIndex] = pde;
    }
    else
    {
        pd = (PageTable*)((uint64_t)pde.Address << 12);
    }

    pde = pd->Entries[ptIndex];
    PageTable* pt;
    if (!pde.Present)
    {
        pt = (PageTable*)page_allocator_request();
        memset(pt, 0, 0x1000);
        pde.Address = (uint64_t)pt >> 12;
        pde.Present = 1;
        pde.ReadWrite = 1;
        pd->Entries[ptIndex] = pde;
    }
    else
    {
        pt = (PageTable*)((uint64_t)pde.Address << 12);
    }

    pde = pt->Entries[pIndex];
    pde.Address = (uint64_t)physicalAddress >> 12;
    pde.Present = 1;
    pde.ReadWrite = 1;
    pt->Entries[pIndex] = pde;
}