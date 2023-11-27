#include "page_table.h"

#include "kernel/page_allocator/page_allocator.h"

#include "libc/include/string.h"

PageTable* PML4;

void page_table_init(Framebuffer* screenbuffer)
{
    PML4 = (PageTable*)page_allocator_request();
    memset(PML4, 0, 4096);

    for (uint64_t i = 0; i < page_allocator_get_total_amount(); i++)
    {
        page_table_map_page((void*)(i * 4096), (void*)(i * 4096));
    }

    for (uint64_t i = 0; i < screenbuffer->Size + 4096; i += 4096)
    {
        page_table_map_page((void*)((uint64_t)screenbuffer->Base + i), (void*)((uint64_t)screenbuffer->Base + i));
    }

    asm volatile ("mov %0, %%cr3" : : "r" (PML4));
}

void page_table_map_page(void* virtualAddress, void* physicalAddress)
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

    pde = PML4->Entries[pdpIndex];
    PageTable* pdp;
    if (!pde.Present)
    {
        pdp = (PageTable*)page_allocator_request();
        memset(pdp, 0, 0x1000);
        pde.Address = (uint64_t)pdp >> 12;
        pde.Present = 1;
        pde.ReadWrite = 1;
        PML4->Entries[pdpIndex] = pde;
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