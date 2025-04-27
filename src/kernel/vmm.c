#include "vmm.h"
#include "lock.h"
#include "log.h"
#include "pmm.h"
#include "regs.h"
#include "sched.h"
#include "space.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

static lock_t kernelLock;
static pml_t* kernelPml;

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
    if (kernelPml == NULL)
    {
        log_panic(NULL, "Failed to allocate kernel PML");
    }

    memset(kernelPml, 0, PAGE_SIZE);

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        uint64_t result =
            pml_map(kernelPml, desc->virtualStart, desc->physicalStart, desc->amountOfPages, PAGE_WRITE | VMM_KERNEL_PAGES);
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

    printf("vmm: kernel phys=[0x%016lx-0x%016lx] virt=[0x%016lx-0x%016lx]", kernel->physStart, kernel->physStart + kernel->length,
        kernel->virtStart, kernel->virtStart + kernel->length);

    uint64_t result =
        pml_map(kernelPml, kernel->virtStart, kernel->physStart, SIZE_IN_PAGES(kernel->length), PAGE_WRITE | VMM_KERNEL_PAGES);
    if (result == ERR)
    {
        log_panic(NULL, "Failed to map kernel");
    }

    printf("vmm: loading pml 0x%016lx", kernelPml);
    pml_load(kernelPml);
    printf("vmm: pml loaded");

    gopBuffer->base = vmm_kernel_map(NULL, gopBuffer->base, gopBuffer->size);
    if (gopBuffer->base == NULL)
    {
        log_panic(NULL, "Failed to map GOP buffer");
    }

    vmm_cpu_init();
}

void vmm_cpu_init(void)
{
    printf("vmm: global page enable");
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

        if (page == NULL || pml_map(kernelPml, addr, VMM_HIGHER_TO_LOWER(page), 1, PAGE_WRITE | VMM_KERNEL_PAGES) == ERR)
        {
            if (page != NULL)
            {
                pmm_free(page);
            }

            for (uint64_t j = 0; j < i; j++)
            {
                void* otherAddr = (void*)((uint64_t)virtAddr + j * PAGE_SIZE);
                pmm_free(VMM_LOWER_TO_HIGHER(pml_phys_addr(kernelPml, otherAddr)));
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
        printf("vmm: map lower [0x%016lx-0x%016lx] to higher", physAddr, ((uintptr_t)physAddr) + length);
    }

    if (!pml_region_unmapped(kernelPml, virtAddr, SIZE_IN_PAGES(length)))
    {
        printf("vmm_kernel_map: already mapped");
        return ERRPTR(EEXIST);
    }

    uint64_t result = pml_map(kernelPml, virtAddr, physAddr, SIZE_IN_PAGES(length), PAGE_WRITE | VMM_KERNEL_PAGES);
    if (result == ERR)
    {
        printf("vmm: failed to map kernel memory");
        return NULL;
    }

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
                pmm_free(VMM_LOWER_TO_HIGHER(pml_phys_addr(space->pml, otherAddr)));
                pml_unmap(space->pml, otherAddr, 1);
            }
            return ERRPTR(ENOMEM);
        }
    }

    return virtAddr;
}

void* vmm_map(void* virtAddr, void* physAddr, uint64_t length, prot_t prot)
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

    physAddr = (void*)ROUND_DOWN(physAddr, PAGE_SIZE);
    vmm_align_region(&virtAddr, &length);

    if (!pml_region_unmapped(space->pml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERRPTR(EEXIST);
    }

    if (pml_map(space->pml, virtAddr, physAddr, SIZE_IN_PAGES(length), flags) == ERR)
    {
        return ERRPTR(ENOMEM);
    }

    return virtAddr;
}

uint64_t vmm_unmap(void* virtAddr, uint64_t length)
{
    vmm_align_region(&virtAddr, &length);
    space_t* space = &sched_process()->space;
    LOCK_DEFER(&space->lock);

    if (pml_region_unmapped(space->pml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERROR(EFAULT);
    }

    pml_unmap(space->pml, virtAddr, SIZE_IN_PAGES(length));
    return 0;
}

uint64_t vmm_protect(void* virtAddr, uint64_t length, prot_t prot)
{
    uint64_t flags = vmm_prot_to_flags(prot);
    if (flags == ERR)
    {
        return ERROR(EINVAL);
    }

    vmm_align_region(&virtAddr, &length);
    space_t* space = &sched_process()->space;
    LOCK_DEFER(&space->lock);

    if (pml_region_unmapped(space->pml, virtAddr, SIZE_IN_PAGES(length)))
    {
        return ERROR(EFAULT);
    }

    uint64_t result = pml_change_flags(space->pml, virtAddr, SIZE_IN_PAGES(length), flags);
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
