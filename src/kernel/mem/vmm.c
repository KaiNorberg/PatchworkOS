#include "vmm.h"

#include "cpu/regs.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/space.h"
#include "pmm.h"
#include "sched/thread.h"
#include "sync/lock.h"

#include <common/paging.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
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
        void* firstMappedPage =
            page_table_find_first_mapped_page(&kernelPageTable, (void*)addr, (void*)(addr + pageAmount * PAGE_SIZE));

        if (firstMappedPage == NULL)
        {
            // Found a free region
            kernelFreeAddress = addr + pageAmount * PAGE_SIZE;
            return (void*)addr;
        }

        // Skip to after the mapped page and continue searching
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

static void vmm_load_memory_map(efi_mem_map_t* memoryMap)
{
    // Kernel pml must be within 32 bit boundary because smp trampoline loads it as a dword.
    kernelPageTable.pml4 = pmm_alloc_bitmap(1, UINT32_MAX, 0);
    if (kernelPageTable.pml4 == NULL)
    {
        panic(NULL, "Failed to allocate kernel PML");
    }
    kernelPageTable.allocPage = pmm_alloc;
    kernelPageTable.freePage = pmm_free;

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        uint64_t result = page_table_map(&kernelPageTable, desc->virtualStart, desc->physicalStart, desc->amountOfPages,
            PML_WRITE | VMM_KERNEL_PML_FLAGS, PML_CALLBACK_NONE);
        if (result == ERR)
        {
            const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
            panic(NULL, "Failed to map memory descriptor %d (phys=0x%016lx-0x%016lx virt=0x%016lx)", i,
                desc->physicalStart, desc->physicalStart + desc->amountOfPages * PAGE_SIZE, desc->virtualStart);
        }
    }
}

void vmm_init(efi_mem_map_t* memoryMap, boot_kernel_t* kernel, gop_buffer_t* gopBuffer)
{
    lock_init(&kernelLock);
    vmm_load_memory_map(memoryMap);

    kernelFreeAddress = ROUND_UP((uintptr_t)&_kernelEnd, PAGE_SIZE);

    LOG_INFO("kernel phys=[0x%016lx-0x%016lx] virt=[0x%016lx-0x%016lx]\n", kernel->physStart,
        kernel->physStart + kernel->length, kernel->virtStart, kernel->virtStart + kernel->length);

    uint64_t result = page_table_map(&kernelPageTable, kernel->virtStart, kernel->physStart,
        BYTES_TO_PAGES(kernel->length), PML_WRITE | VMM_KERNEL_PML_FLAGS, PML_CALLBACK_NONE);
    if (result == ERR)
    {
        panic(NULL, "Failed to map kernel (phys=0x%016lx-0x%016lx virt=0x%016lx)", kernel->physStart,
            kernel->physStart + kernel->length, kernel->virtStart);
    }

    LOG_INFO("loading kernel pml 0x%016lx\n", kernelPageTable.pml4);
    page_table_load(&kernelPageTable);
    LOG_INFO("kernel pml loaded\n");

    gopBuffer->base = vmm_kernel_map(NULL, gopBuffer->base, BYTES_TO_PAGES(gopBuffer->size), PML_WRITE);
    if (gopBuffer->base == NULL)
    {
        panic(NULL, "Failed to map GOP buffer (phys=0x%016lx size=%llu)\n", (uintptr_t)gopBuffer->base,
            gopBuffer->size);
    }

    vmm_cpu_init();
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
                    flags | VMM_KERNEL_PML_FLAGS | PML_OWNED, PML_CALLBACK_NONE) == ERR)
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

        if (page_table_map(&kernelPageTable, virtAddr, physAddr, pageAmount, flags | VMM_KERNEL_PML_FLAGS,
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
            page_table_map(&space->pageTable, addr, PML_HIGHER_TO_LOWER(page), 1, mapping.flags | PML_OWNED, PML_CALLBACK_NONE) ==
                ERR)
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

    if (page_table_map(&space->pageTable, mapping.virtAddr, mapping.physAddr, mapping.pageAmount, mapping.flags, callbackId) == ERR)
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

        if (page_table_map(&space->pageTable, (void*)((uint64_t)mapping.virtAddr + i * PAGE_SIZE), physAddr, 1, mapping.flags,
                callbackId) == ERR)
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
