#include "vmm.h"

#include "log/log.h"
#include "log/panic.h"
#include "mem/space.h"
#include "pmm.h"
#include "sync/lock.h"

#include <boot/boot_info.h>
#include <common/paging.h>
#include <common/regs.h>

#include <assert.h>
#include <errno.h>
#include <sys/math.h>
#include <sys/proc.h>

extern uint64_t _kernelEnd;

static lock_t kernelLock;
static page_table_t kernelPageTable;
static uintptr_t kernelFreeAddress;

static void* vmm_kernel_find_free_region(uint64_t pageAmount)
{
    uintptr_t addr = kernelFreeAddress;

    while (addr + pageAmount * PAGE_SIZE <= PML_HIGHER_HALF_END)
    {
        void* firstMappedPage;
        if (page_table_find_first_mapped_page(&kernelPageTable, (void*)addr, (void*)(addr + pageAmount * PAGE_SIZE), &firstMappedPage) == ERR)
        {
            kernelFreeAddress = addr + pageAmount * PAGE_SIZE;
            return (void*)addr;
        }

        addr = (uintptr_t)firstMappedPage + PAGE_SIZE;
    }

    return NULL;
}

static void vmm_kernel_update_free_address(uintptr_t virtAddr, uint64_t pageAmount)
{
    if (virtAddr >= ((uintptr_t)&_kernelEnd) && virtAddr < PML_HIGHER_HALF_END)
    {
        if (virtAddr <= kernelFreeAddress && kernelFreeAddress < virtAddr + pageAmount * PAGE_SIZE)
        {
            kernelFreeAddress = virtAddr + pageAmount * PAGE_SIZE;
        }
    }
}

void vmm_init(boot_memory_t* memory, boot_gop_t* gop, boot_kernel_t* kernel)
{
    kernelFreeAddress = ROUND_UP((uintptr_t)&_kernelEnd, PAGE_SIZE);
    lock_init(&kernelLock);

    // Kernel pml must be within 32 bit boundary because smp trampoline loads it as a dword.
    kernelPageTable.pml4 = pmm_alloc_bitmap(1, UINT32_MAX, 0);
    if (kernelPageTable.pml4 == NULL)
    {
        panic(NULL, "Failed to allocate kernel PML");
    }
    memset(kernelPageTable.pml4, 0, sizeof(pml_t));
    kernelPageTable.allocPage = pmm_alloc;
    kernelPageTable.freePage = pmm_free;

    for (uint64_t i = 0; i < memory->map.length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(&memory->map, i);
        if (page_table_map(&kernelPageTable, (void*)desc->PhysicalStart, (void*)desc->PhysicalStart, desc->NumberOfPages, PML_WRITE,
                PML_CALLBACK_NONE) == ERR)
        {
            panic(NULL, "Failed to map memory descriptor %d (phys=0x%016lx-0x%016lx virt=0x%016lx)", i, desc->PhysicalStart,
                desc->PhysicalStart + desc->NumberOfPages * PAGE_SIZE, desc->VirtualStart);
        }
        if (page_table_map(&kernelPageTable, (void*)desc->VirtualStart, (void*)desc->PhysicalStart, desc->NumberOfPages, PML_WRITE,
                PML_CALLBACK_NONE) == ERR)
        {
            panic(NULL, "Failed to map memory descriptor %d (phys=0x%016lx-0x%016lx virt=0x%016lx)", i, desc->PhysicalStart,
                desc->PhysicalStart + desc->NumberOfPages * PAGE_SIZE, desc->VirtualStart);
        }
    }

    LOG_INFO("kernel virt=[0x%016lx-0x%016lx] phys=[0x%016lx-0x%016lx]\n", kernel->virtStart, kernel->virtStart + kernel->size, kernel->physStart, kernel->physStart + kernel->size);
    if (page_table_map(&kernelPageTable, (void*)kernel->virtStart, (void*)kernel->physStart, BYTES_TO_PAGES(kernel->size), PML_WRITE,
            PML_CALLBACK_NONE) == ERR)
    {
        panic(NULL, "Failed to map kernel memory");
    }

    LOG_INFO("   GOP virt=[0x%016lx-0x%016lx] phys=[0x%016lx-0x%016lx]\n", gop->virtAddr, gop->virtAddr + gop->size, gop->physAddr, gop->physAddr + gop->size);
    if (page_table_map(&kernelPageTable, (void*)gop->virtAddr, (void*)gop->physAddr, BYTES_TO_PAGES(gop->size), PML_WRITE,
            PML_CALLBACK_NONE) == ERR)
    {
        panic(NULL, "Failed to map GOP memory");
    }

    vmm_cpu_init();

    LOG_INFO("loading kernel page table... ");
    page_table_load(&kernelPageTable);
    LOG_INFO("done!\n");
}

void vmm_cpu_init(void)
{
    LOG_INFO("global page enable\n");
    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);
}

page_table_t* vmm_kernel_pml(void)
{
    return &kernelPageTable;
}

void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t pageAmount, pml_flags_t flags)
{
    LOCK_SCOPE(&kernelLock);

    // Sanity checks, if the kernel specifies weird values we assume some corruption or fundamental failure has
    // taken place.
    assert((uintptr_t)virtAddr % PAGE_SIZE == 0);
    assert((uintptr_t)physAddr % PAGE_SIZE == 0);
    assert(pageAmount != 0);

    if (physAddr == NULL)
    {
        if (virtAddr == NULL)
        {
            virtAddr = vmm_kernel_find_free_region(pageAmount);
            if (virtAddr == NULL)
            {
                errno = ENOMEM;
                return NULL;
            }
        }
        else
        {
            if (!page_table_is_unmapped(&kernelPageTable, virtAddr, pageAmount))
            {
                errno = EEXIST;
                return NULL;
            }
        }

        for (uint64_t i = 0; i < pageAmount; i++)
        {
            void* page = pmm_alloc();
            void* vaddr = (void*)((uint64_t)virtAddr + i * PAGE_SIZE);

            if (page == NULL ||
                page_table_map(&kernelPageTable, vaddr, PML_HIGHER_TO_LOWER(page), 1,
                    flags | PML_KERNEL_FLAGS | PML_OWNED, PML_CALLBACK_NONE) == ERR)
            {
                if (page != NULL)
                {
                    pmm_free(page);
                }

                // Page table will free the previously allocated pages as they are owned by the Page table.
                page_table_unmap(&kernelPageTable, virtAddr, i);
                LOG_WARN("failed to map kernel page at virt=0x%016lx (attempt %llu/%llu)\n", (uintptr_t)vaddr, i + 1,
                    pageAmount);
                errno = ENOMEM;
                return NULL;
            }
        }

        vmm_kernel_update_free_address((uintptr_t)virtAddr, pageAmount);
    }
    else
    {
        physAddr = PML_ENSURE_LOWER_HALF(physAddr);
        if (virtAddr == NULL)
        {
            virtAddr = PML_LOWER_TO_HIGHER(physAddr);
            LOG_DEBUG("map lower [0x%016lx-0x%016lx] to higher\n", physAddr,
                (uintptr_t)physAddr + pageAmount * PAGE_SIZE);
        }

        if (!page_table_is_unmapped(&kernelPageTable, virtAddr, pageAmount))
        {
            errno = EEXIST;
            return NULL;
        }

        if (page_table_map(&kernelPageTable, virtAddr, physAddr, pageAmount, flags | PML_KERNEL_FLAGS,
                PML_CALLBACK_NONE) == ERR)
        {
            errno = ENOMEM;
            return NULL;
        }
    }

    return virtAddr;
}

void vmm_kernel_unmap(void* virtAddr, uint64_t pageAmount)
{
    LOCK_SCOPE(&kernelLock);

    // Sanity checks, if the kernel specifies weird values we assume some corruption or fundamental failure has taken
    // place.
    assert((uintptr_t)virtAddr % 0x1000 == 0);
    assert(pageAmount != 0);

    page_table_unmap(&kernelPageTable, virtAddr, pageAmount);
}

void vmm_align_region(void** virtAddr, uint64_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uint64_t)*virtAddr - (uint64_t)aligned);
    *virtAddr = aligned;
}

void* vmm_alloc(space_t* space, void* virtAddr, uint64_t length, prot_t prot)
{
    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, NULL, length, prot) == ERR)
    {
        return NULL;
    }

    for (uint64_t i = 0; i < mapping.pageAmount; i++)
    {
        void* addr = (void*)((uint64_t)mapping.virtAddr + i * PAGE_SIZE);
        void* page = pmm_alloc();

        if (page == NULL ||
            page_table_map(&space->pageTable, addr, PML_HIGHER_TO_LOWER(page), 1, mapping.flags | PML_OWNED,
                PML_CALLBACK_NONE) == ERR)
        {
            if (page != NULL)
            {
                pmm_free(page);
            }

            // Page table will free the previously allocated pages as they are owned by the Page table.
            page_table_unmap(&space->pageTable, mapping.virtAddr, i);
            return space_mapping_end(space, &mapping, ENOMEM);
        }
    }

    return space_mapping_end(space, &mapping, 0);
}

void* vmm_map(space_t* space, void* virtAddr, void* physAddr, uint64_t length, prot_t prot, space_callback_func_t func,
    void* private)
{
    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, physAddr, length, prot) == ERR)
    {
        return NULL;
    }

    pml_callback_id_t callbackId = PML_CALLBACK_NONE;
    if (func != NULL)
    {
        callbackId = space_alloc_callback(space, mapping.pageAmount, func, private);
        if (callbackId == PML_MAX_CALLBACK)
        {
            return space_mapping_end(space, &mapping, ENOSPC);
        }
    }

    if (page_table_map(&space->pageTable, mapping.virtAddr, mapping.physAddr, mapping.pageAmount, mapping.flags,
            callbackId) == ERR)
    {
        if (callbackId != PML_CALLBACK_NONE)
        {
            space_free_callback(space, callbackId);
        }

        return space_mapping_end(space, &mapping, ENOMEM);
    }

    return space_mapping_end(space, &mapping, 0);
}

void* vmm_map_pages(space_t* space, void* virtAddr, void** pages, uint64_t pageAmount, prot_t prot,
    space_callback_func_t func, void* private)
{
    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, NULL, pageAmount * PAGE_SIZE, prot) == ERR)
    {
        return NULL;
    }

    pml_callback_id_t callbackId = PML_CALLBACK_NONE;
    if (func != NULL)
    {
        callbackId = space_alloc_callback(space, pageAmount, func, private);
        if (callbackId == PML_MAX_CALLBACK)
        {
            return space_mapping_end(space, &mapping, ENOSPC);
        }
    }

    for (uint64_t i = 0; i < mapping.pageAmount; i++)
    {
        void* physAddr = PML_ENSURE_LOWER_HALF(pages[i]);

        if (page_table_map(&space->pageTable, (void*)((uint64_t)mapping.virtAddr + i * PAGE_SIZE), physAddr, 1,
                mapping.flags, callbackId) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                page_table_unmap(&space->pageTable, (void*)((uint64_t)mapping.virtAddr + j * PAGE_SIZE), 1);
            }

            if (callbackId != PML_CALLBACK_NONE)
            {
                space_free_callback(space, callbackId);
            }

            return space_mapping_end(space, &mapping, ENOMEM);
        }
    }

    return space_mapping_end(space, &mapping, 0);
}
