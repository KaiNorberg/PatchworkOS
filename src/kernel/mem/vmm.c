#include "vmm.h"
#include "cpu/regs.h"
#include "pmm.h"
#include "sched/sched.h"
#include "sync/lock.h"
#include "utils/log.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

static lock_t kernelLock;
static pml_t* kernelPml;

void space_init(space_t* space)
{
    space->pml = pml_new();
    space->freeAddress = 0x400000;
    list_init(&space->mappedRegions);
    lock_init(&space->lock);

    pml_t* kernelPml = vmm_kernel_pml();
    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pml->entries[i] = kernelPml->entries[i];
    }
}

void space_deinit(space_t* space)
{
    for (uint64_t i = PAGE_ENTRY_AMOUNT / 2; i < PAGE_ENTRY_AMOUNT; i++)
    {
        space->pml->entries[i] = (pml_entry_t){0};
    }

    pml_free(space->pml);

    mapped_region_t* region;
    mapped_region_t* temp;
    LIST_FOR_EACH_SAFE(region, temp, &space->mappedRegions, entry)
    {
        region->callback(region->private);
        free(region);
    }
}

void space_load(space_t* space)
{
    if (space == NULL)
    {
        pml_load(vmm_kernel_pml());
    }
    else
    {
        pml_load(space->pml);
    }
}

static void* vmm_find_free_region(space_t* space, uint64_t length)
{
    uint64_t pageAmount = SIZE_IN_PAGES(length);
    for (uintptr_t addr = space->freeAddress; addr < ROUND_DOWN(UINT64_MAX, 0x1000); addr += pageAmount * PAGE_SIZE)
    {
        if (pml_region_unmapped(space->pml, (void*)addr, pageAmount))
        {
            space->freeAddress = addr + pageAmount * PAGE_SIZE;
            return (void*)addr;
        }
    }
    log_panic(NULL, "Address space filled, you must have ran this on a super computer... dont do that.");
}

static void vmm_align_region(void** virtAddr, uint64_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uint64_t)*virtAddr - (uint64_t)aligned);
    *virtAddr = aligned;
}

static uint64_t vmm_prot_to_flags(prot_t prot)
{
    if (!(prot & PROT_READ))
    {
        return ERR;
    }
    return (prot & PROT_WRITE ? PAGE_WRITE : 0) | PAGE_USER;
}

static void vmm_load_memory_map(efi_mem_map_t* memoryMap)
{
    // Kernel pml must be within 32 bit boundry becouse smp trampline loads it as a dword.
    kernelPml = pmm_alloc_bitmap(1, UINT32_MAX, 0);
    assert(kernelPml != NULL);

    memset(kernelPml, 0, PAGE_SIZE);

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        uint64_t result = pml_map(kernelPml, desc->virtualStart, desc->physicalStart, desc->amountOfPages,
            PAGE_WRITE | VMM_KERNEL_PAGES);
        if (result == ERR)
        {
            log_panic(NULL, "Failed to map memory descriptor %d", i);
        }
    }
}

void vmm_init(efi_mem_map_t* memoryMap, boot_kernel_t* kernel, gop_buffer_t* gopBuffer)
{
    lock_init(&kernelLock);
    vmm_load_memory_map(memoryMap);

    printf("vmm: kernel phys=[0x%016lx-0x%016lx] virt=[0x%016lx-0x%016lx]\n", kernel->physStart,
        kernel->physStart + kernel->length, kernel->virtStart, kernel->virtStart + kernel->length);

    uint64_t result = pml_map(kernelPml, kernel->virtStart, kernel->physStart, SIZE_IN_PAGES(kernel->length),
        PAGE_WRITE | VMM_KERNEL_PAGES);
    if (result == ERR)
    {
        log_panic(NULL, "Failed to map kernel");
    }

    printf("vmm: loading pml 0x%016lx\n", kernelPml);
    pml_load(kernelPml);
    printf("vmm: pml loaded\n");

    gopBuffer->base = vmm_kernel_map(NULL, gopBuffer->base, gopBuffer->size);
    if (gopBuffer->base == NULL)
    {
        log_panic(NULL, "Failed to map GOP buffer");
    }

    vmm_cpu_init();
}

void vmm_cpu_init(void)
{
    printf("vmm: global page enable\n");
    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);
}

pml_t* vmm_kernel_pml(void)
{
    return kernelPml;
}

void* vmm_kernel_alloc(void* virtAddr, uint64_t length)
{
    LOCK_DEFER(&kernelLock);

    if (length == 0)
    {
        return ERRPTR(EINVAL);
    }

    vmm_align_region(&virtAddr, &length);

    if (!pml_region_unmapped(kernelPml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERRPTR(EEXIST);
    }

    for (uint64_t i = 0; i < SIZE_IN_PAGES(length); i++)
    {
        void* addr = (void*)((uint64_t)virtAddr + i * PAGE_SIZE);
        void* page = pmm_alloc();

        if (page == NULL ||
            pml_map(kernelPml, addr, VMM_HIGHER_TO_LOWER(page), 1, PAGE_WRITE | VMM_KERNEL_PAGES) == ERR)
        {
            if (page != NULL)
            {
                pmm_free(page);
            }

            for (uint64_t j = 0; j < i; j++)
            {
                void* otherAddr = (void*)((uint64_t)virtAddr + j * PAGE_SIZE);
                pmm_free(pml_phys_addr(kernelPml, otherAddr));
                pml_unmap(kernelPml, otherAddr, 1);
            }
            return ERRPTR(ENOMEM);
        }
    }

    return virtAddr;
}

void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t length)
{
    LOCK_DEFER(&kernelLock);

    if (length == 0)
    {
        return ERRPTR(EINVAL);
    }

    vmm_align_region(&virtAddr, &length);

    if (virtAddr == NULL)
    {
        virtAddr = VMM_LOWER_TO_HIGHER(physAddr);
        printf("vmm: map lower [0x%016lx-0x%016lx] to higher\n", physAddr, ((uintptr_t)physAddr) + length);
    }
    physAddr = VMM_PHYS_TO_LOWER_SAFE(physAddr);

    assert(pml_region_unmapped(kernelPml, virtAddr, SIZE_IN_PAGES(length)));
    assert(pml_map(kernelPml, virtAddr, physAddr, SIZE_IN_PAGES(length), PAGE_WRITE | VMM_KERNEL_PAGES) != ERR);

    return virtAddr;
}

void* vmm_alloc(void* virtAddr, uint64_t length, prot_t prot)
{
    space_t* space = &sched_process()->space;
    LOCK_DEFER(&space->lock);

    if (length == 0)
    {
        return ERRPTR(EINVAL);
    }

    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    {
        return ERRPTR(EINVAL);
    }
    flags |= PAGE_OWNED;

    if (virtAddr == NULL)
    {
        virtAddr = vmm_find_free_region(space, length);
    }

    vmm_align_region(&virtAddr, &length);

    if (!pml_region_unmapped(space->pml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERRPTR(EEXIST);
    }

    for (uint64_t i = 0; i < SIZE_IN_PAGES(length); i++)
    {
        void* addr = (void*)((uint64_t)virtAddr + i * PAGE_SIZE);
        void* page = pmm_alloc();

        if (page == NULL || pml_map(space->pml, addr, VMM_HIGHER_TO_LOWER(page), 1, flags) == ERR)
        {
            if (page != NULL)
            {
                pmm_free(page);
            }

            for (uint64_t j = 0; j < i; j++)
            {
                void* otherAddr = (void*)((uint64_t)virtAddr + j * PAGE_SIZE);
                pmm_free(pml_phys_addr(space->pml, otherAddr));
                pml_unmap(space->pml, otherAddr, 1);
            }
            return ERRPTR(ENOMEM);
        }
    }

    return virtAddr;
}

void* vmm_map(void* virtAddr, void* physAddr, uint64_t length, prot_t prot, vmm_callback_t callback, void* private)
{
    space_t* space = &sched_process()->space;
    LOCK_DEFER(&space->lock);

    if (physAddr == NULL)
    {
        return ERRPTR(EFAULT);
    }

    if (length == 0)
    {
        return ERRPTR(EINVAL);
    }

    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    {
        return ERRPTR(EACCES);
    }

    if (virtAddr == NULL)
    {
        virtAddr = vmm_find_free_region(space, length);
    }
    physAddr = VMM_PHYS_TO_LOWER_SAFE((void*)ROUND_DOWN(physAddr, PAGE_SIZE));
    vmm_align_region(&virtAddr, &length);

    uint64_t pageAmount = SIZE_IN_PAGES(length);

    if (callback != NULL)
    {
        mapped_region_t* region = malloc(sizeof(mapped_region_t) + BITMAP_BITS_TO_BYTES(pageAmount));
        if (region == NULL)
        {
            return NULL;
        }
        list_entry_init(&region->entry);
        region->callback = callback;
        region->private = private;
        region->start = virtAddr;
        bitmap_init(&region->pages, region->bitBuffer, pageAmount);
        memset(region->bitBuffer, 0xFF, BITMAP_BITS_TO_BYTES(pageAmount));

        if (!pml_region_unmapped(space->pml, virtAddr, pageAmount))
        {
            free(region);
            return ERRPTR(EEXIST);
        }

        if (pml_map(space->pml, virtAddr, physAddr, pageAmount, flags) == ERR)
        {
            free(region);
            return ERRPTR(ENOMEM);
        }

        list_push(&space->mappedRegions, &region->entry);
    }
    else
    {
        if (!pml_region_unmapped(space->pml, virtAddr, pageAmount))
        {
            return ERRPTR(EEXIST);
        }

        if (pml_map(space->pml, virtAddr, physAddr, pageAmount, flags) == ERR)
        {
            return ERRPTR(ENOMEM);
        }
    }

    return virtAddr;
}

void* vmm_map_pages(void* virtAddr, void** pages, uint64_t pageAmount, prot_t prot, vmm_callback_t callback,
    void* private)
{
    space_t* space = &sched_process()->space;
    LOCK_DEFER(&space->lock);

    if (pageAmount == 0)
    {
        return ERRPTR(EINVAL);
    }

    if (pages == NULL)
    {
        return ERRPTR(EFAULT);
    }

    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    {
        return ERRPTR(EACCES);
    }

    uint64_t length = pageAmount * PAGE_SIZE;
    if (virtAddr == NULL)
    {
        virtAddr = vmm_find_free_region(space, length);
    }
    vmm_align_region(&virtAddr, &length);

    mapped_region_t* region = NULL;
    if (callback != NULL)
    {
        region = malloc(sizeof(mapped_region_t) + BITMAP_BITS_TO_BYTES(pageAmount));
        if (region == NULL)
        {
            return NULL;
        }
        list_entry_init(&region->entry);
        region->callback = callback;
        region->private = private;
        region->start = virtAddr;
        bitmap_init(&region->pages, region->bitBuffer, pageAmount);
        memset(region->bitBuffer, 0xFF, BITMAP_BITS_TO_BYTES(pageAmount));

        if (!pml_region_unmapped(space->pml, virtAddr, pageAmount))
        {
            free(region);
            return ERRPTR(EEXIST);
        }

        for (uint64_t i = 0; i < pageAmount; i++)
        {
            void* physAddr = VMM_PHYS_TO_LOWER_SAFE(pages[i]);

            if (pml_map(space->pml, (void*)((uint64_t)virtAddr + i * PAGE_SIZE), physAddr, 1, flags) == ERR)
            {
                for (uint64_t j = 0; j < i; j++)
                {
                    pml_unmap(space->pml, (void*)((uint64_t)virtAddr + j * PAGE_SIZE), 1);
                }
                free(region);
                return ERRPTR(ENOMEM);
            }
        }

        list_push(&space->mappedRegions, &region->entry);
    }
    else
    {
        if (!pml_region_unmapped(space->pml, virtAddr, pageAmount))
        {
            return ERRPTR(EEXIST);
        }

        for (uint64_t i = 0; i < pageAmount; i++)
        {
            void* physAddr = VMM_PHYS_TO_LOWER_SAFE(pages[i]);

            if (pml_map(space->pml, (void*)((uint64_t)virtAddr + i * PAGE_SIZE), physAddr, 1, flags) == ERR)
            {
                for (uint64_t j = 0; j < i; j++)
                {
                    pml_unmap(space->pml, (void*)((uint64_t)virtAddr + j * PAGE_SIZE), 1);
                }

                return ERRPTR(ENOMEM);
            }
        }
    }

    return virtAddr;
}

uint64_t vmm_unmap(void* virtAddr, uint64_t length)
{
    space_t* space = &sched_process()->space;
    LOCK_DEFER(&space->lock);

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = SIZE_IN_PAGES(length);

    if (pml_region_unmapped(space->pml, virtAddr, pageAmount))
    {
        return ERROR(EFAULT);
    }

    pml_unmap(space->pml, virtAddr, pageAmount);

    mapped_region_t* region;
    mapped_region_t* temp;
    LIST_FOR_EACH_SAFE(region, temp, &space->mappedRegions, entry)
    {
        void* start = region->start;
        void* end = start + region->pages.length * PAGE_SIZE;

        void* overlapStart = MAX(start, virtAddr);
        void* overlapEnd = MIN(end, virtAddr + pageAmount * PAGE_SIZE);

        if (overlapStart > overlapEnd) // No overlap
        {
            continue;
        }

        uint64_t startIndex = (uint64_t)(overlapStart - start) / PAGE_SIZE;
        uint64_t endIndex = (uint64_t)(overlapEnd - start) / PAGE_SIZE;
        bitmap_clear(&region->pages, startIndex, endIndex);

        if (bitmap_sum(&region->pages, 0, region->pages.length) == 0)
        {
            list_remove(&region->entry);
            region->callback(region->private);
            free(region);
        }
    }
    return 0;
}

uint64_t vmm_protect(void* virtAddr, uint64_t length, prot_t prot)
{
    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    {
        return ERROR(EINVAL);
    }

    space_t* space = &sched_process()->space;
    LOCK_DEFER(&space->lock);

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = SIZE_IN_PAGES(length);

    if (pml_region_unmapped(space->pml, virtAddr, pageAmount))
    {
        return ERROR(EFAULT);
    }

    uint64_t result = pml_change_flags(space->pml, virtAddr, pageAmount, flags);
    if (result == ERR)
    {
        return ERROR(ENOMEM);
    }

    return 0;
}

bool vmm_mapped(const void* virtAddr, uint64_t length)
{
    vmm_align_region((void**)&virtAddr, &length);
    space_t* space = &sched_process()->space;
    LOCK_DEFER(&space->lock);
    return pml_region_mapped(space->pml, virtAddr, SIZE_IN_PAGES(length));
}
