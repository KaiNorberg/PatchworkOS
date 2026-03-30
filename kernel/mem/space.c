#include <kernel/cpu/ipi.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/space.h>

#include <kernel/cpu/cpu.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/space.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/clock.h>

#include <assert.h>
#include <libc/map.h>
#include <libc/math.h>
#include <libc/proc.h>
#include <stdlib.h>
#include <string.h>

static bool space_pmm_bitmap_alloc_pages(pfn_t* pfns, size_t pageAmount)
{
    for (size_t i = 0; i < pageAmount; i++)
    {
        pfn_t pfn = pmm_alloc_bitmap(1, UINT32_MAX, 0);
        if (pfn == PFN_INVALID)
        {
            for (size_t j = 0; j < i; j++)
            {
                pmm_free(pfns[j]);
            }
            return false;
        }
        pfns[i] = pfn;
    }
    return true;
}

static inline void space_map_kernel_space_region(space_t* space, uintptr_t start, uintptr_t end)
{
    space_t* kernelSpace = vmm_kernel_space_get();
    assert(kernelSpace != NULL);

    pml_index_t startIndex = PML_ADDR_TO_INDEX(start, PML4);
    pml_index_t endIndex = PML_ADDR_TO_INDEX(end - 1, PML4) + 1; // Inclusive end

    for (pml_index_t i = startIndex; i < endIndex; i++)
    {
        space->pageTable.pml4->entries[i] = kernelSpace->pageTable.pml4->entries[i];
        space->pageTable.pml4->entries[i].owned = 0;
    }
}

static inline void space_unmap_kernel_space_region(space_t* space, uintptr_t start, uintptr_t end)
{
    pml_index_t startIndex = PML_ADDR_TO_INDEX(start, PML4);
    pml_index_t endIndex = PML_ADDR_TO_INDEX(end - 1, PML4) + 1; // Inclusive end

    for (pml_index_t i = startIndex; i < endIndex; i++)
    {
        space->pageTable.pml4->entries[i].raw = 0;
    }
}

status_t space_init(space_t* space, uintptr_t startAddress, uintptr_t endAddress, space_flags_t flags)
{
    if (space == NULL)
    {
        return ERR(MMU, INVAL);
    }

    if (flags & SPACE_USE_PMM_BITMAP)
    {
        if (!page_table_init(&space->pageTable, space_pmm_bitmap_alloc_pages, pmm_free_pages))
        {
            return ERR(MMU, NOMEM);
        }
        // We only use the bitmap pmm allocator for the page table itself, not for mappings.
        space->pageTable.allocPages = pmm_alloc_pages;
    }
    else
    {
        if (!page_table_init(&space->pageTable, pmm_alloc_pages, pmm_free_pages))
        {
            return ERR(MMU, NOMEM);
        }
    }

    space->startAddress = startAddress;
    space->endAddress = endAddress;
    space->freeAddress = startAddress;
    space->flags = flags;
    space->callbacks = NULL;
    space->callbacksLength = 0;
    BITMAP_DEFINE_INIT(space->callbackBitmap, PML_MAX_CALLBACK);
    BITMAP_DEFINE_INIT(space->cpus, CPU_MAX);
    atomic_init(&space->shootdownAcks, 0);
    lock_init(&space->cpuLock);
    lock_init(&space->lock);

    if (flags & SPACE_MAP_KERNEL_BINARY)
    {
        space_map_kernel_space_region(space, VMM_KERNEL_BINARY_MIN, VMM_KERNEL_BINARY_MAX);
    }

    if (flags & SPACE_MAP_KERNEL_HEAP)
    {
        space_map_kernel_space_region(space, VMM_KERNEL_HEAP_MIN, VMM_KERNEL_HEAP_MAX);
    }

    if (flags & SPACE_MAP_IDENTITY)
    {
        space_map_kernel_space_region(space, VMM_IDENTITY_MAPPED_MIN, VMM_IDENTITY_MAPPED_MAX);
    }

    return OK;
}

void space_deinit(space_t* space)
{
    if (space == NULL)
    {
        return;
    }

    if (!bitmap_is_empty(&space->cpus))
    {
        panic(NULL, "Attempted to free address space still in use by CPUs");
    }

    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        space->callbacks[index].func(space->callbacks[index].data);
    }

    if (space->flags & SPACE_MAP_KERNEL_BINARY)
    {
        space_unmap_kernel_space_region(space, VMM_KERNEL_BINARY_MIN, VMM_KERNEL_BINARY_MAX);
    }

    if (space->flags & SPACE_MAP_KERNEL_HEAP)
    {
        space_unmap_kernel_space_region(space, VMM_KERNEL_HEAP_MIN, VMM_KERNEL_HEAP_MAX);
    }

    if (space->flags & SPACE_MAP_IDENTITY)
    {
        space_unmap_kernel_space_region(space, VMM_IDENTITY_MAPPED_MIN, VMM_IDENTITY_MAPPED_MAX);
    }

    free(space->callbacks);
    page_table_deinit(&space->pageTable);
}

bool space_check_access(space_t* space, const void* addr, size_t length)
{
    if (space == NULL || (addr == NULL && length != 0))
    {
        return false;
    }

    if (length == 0)
    {
        return true;
    }

    uintptr_t addrOverflow = (uintptr_t)addr + length;
    if (addrOverflow < (uintptr_t)addr)
    {
        return false;
    }

    if ((uintptr_t)addr < space->startAddress || addrOverflow > space->endAddress)
    {
        return false;
    }

    return true;
}

static void space_align_region(void** virtAddr, size_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uintptr_t)*virtAddr - (uintptr_t)aligned);
    *virtAddr = aligned;
}

bool space_is_mapped(space_t* space, const void* virtAddr, size_t length)
{
    space_align_region((void**)&virtAddr, &length);
    LOCK_SCOPE(&space->lock);
    return page_table_is_mapped(&space->pageTable, virtAddr, BYTES_TO_PAGES(length));
}

uint64_t space_user_page_count(space_t* space)
{
    if (space == NULL)
    {
        return 0;
    }

    LOCK_SCOPE(&space->lock);
    return page_table_count_pages_with_flags(&space->pageTable, (void*)VMM_USER_SPACE_MIN,
        BYTES_TO_PAGES(VMM_USER_SPACE_MAX - VMM_USER_SPACE_MIN), PML_PRESENT | PML_USER | PML_OWNED);
}

status_t space_virt_to_phys(space_t* space, const void* virtAddr, phys_addr_t* out)
{
    if (space == NULL)
    {
        return ERR(MMU, INVAL);
    }

    LOCK_SCOPE(&space->lock);
    if (!page_table_get_phys_addr(out, &space->pageTable, (void*)virtAddr))
    {
        return ERR(MMU, FAULT);
    }

    return OK;
}

status_t space_virt_to_phys_alloc(space_t* space, const void* virtAddr, phys_addr_t* out)
{
    if (space == NULL || out == NULL)
    {
        return ERR(MMU, INVAL);
    }

    status_t status = space_virt_to_phys(space, virtAddr, out);
    if (IS_INFO(status))
    {
        return OK;
    }

    void* alignedAddr = (void*)ROUND_DOWN(virtAddr, PAGE_SIZE);
    status = vmm_alloc(space, &alignedAddr, PAGE_SIZE, PAGE_SIZE, PML_PRESENT | PML_USER | PML_WRITE,
        VMM_ALLOC_ZERO | VMM_ALLOC_FAIL_IF_MAPPED);
    if (IS_ERR(status) && !IS_CODE(status, MAPPED))
    {
        return status;
    }

    return space_virt_to_phys(space, virtAddr, out);
}

status_t space_copy_out(space_t* space, void* dest, const void* src, size_t size)
{
    const uint8_t* ptr = src;
    uint8_t* dst = dest;
    size_t remaining = size;

    while (remaining > 0)
    {
        phys_addr_t phys;
        status_t status = space_virt_to_phys(space, ptr, &phys);
        if (IS_ERR(status))
        {
            return status;
        }

        size_t offset = phys % PAGE_SIZE;
        size_t len = MIN(remaining, PAGE_SIZE - offset);

        void* kaddr = PFN_TO_VIRT(PHYS_TO_PFN(phys)) + offset;
        memcpy(dst, kaddr, len);

        ptr += len;
        dst += len;
        remaining -= len;
    }

    return OK;
}

status_t space_copy_in(space_t* space, void* dest, const void* src, size_t size)
{
    uint8_t* ptr = dest;
    const uint8_t* source = src;
    size_t remaining = size;

    while (remaining > 0)
    {
        phys_addr_t phys;
        status_t status = space_virt_to_phys(space, ptr, &phys);
        if (IS_ERR(status))
        {
            return status;
        }

        size_t offset = phys % PAGE_SIZE;
        size_t len = MIN(remaining, PAGE_SIZE - offset);

        void* kaddr = PFN_TO_VIRT(PHYS_TO_PFN(phys)) + offset;
        memcpy(kaddr, source, len);

        ptr += len;
        source += len;
        remaining -= len;
    }

    return OK;
}