#include "vmm.h"

#include "cpu/syscalls.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/space.h"
#include "pmm.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "sync/rwmutex.h"
#include "utils/bitmap.h"

#include <boot/boot_info.h>
#include <common/paging.h>
#include <common/regs.h>

#include <assert.h>
#include <errno.h>
#include <sys/math.h>
#include <sys/proc.h>

static space_t kernelSpace;

void vmm_init(const boot_memory_t* memory, const boot_gop_t* gop, const boot_kernel_t* kernel)
{
    if (space_init(&kernelSpace, VMM_KERNEL_HEAP_MIN, VMM_KERNEL_HEAP_MAX, SPACE_USE_PMM_BITMAP) == ERR)
    {
        panic(NULL, "Failed to initialize kernel address space");
    }

    LOG_DEBUG("address space layout:\n");
    LOG_DEBUG("  kernel binary:    0x%016lx-0x%016lx\n", VMM_KERNEL_BINARY_MIN, VMM_KERNEL_BINARY_MAX);
    LOG_DEBUG("  kernel stacks:    0x%016lx-0x%016lx\n", VMM_KERNEL_STACKS_MIN, VMM_KERNEL_STACKS_MAX);
    LOG_DEBUG("  kernel heap:      0x%016lx-0x%016lx\n", VMM_KERNEL_HEAP_MIN, VMM_KERNEL_HEAP_MAX);
    LOG_DEBUG("  identity map:     0x%016lx-0x%016lx\n", VMM_IDENTITY_MAPPED_MIN, VMM_IDENTITY_MAPPED_MAX);
    LOG_DEBUG("  user space:       0x%016lx-0x%016lx\n", VMM_USER_SPACE_MIN, VMM_USER_SPACE_MAX);

    LOG_INFO("kernel pml4 allocated at 0x%lx\n", kernelSpace.pageTable.pml4);

    // Keep using the bootloaders memory mappings during initialization.
    for (pml_index_t i = PML_INDEX_LOWER_HALF_MIN; i < PML_INDEX_LOWER_HALF_MAX; i++)
    {
        kernelSpace.pageTable.pml4->entries[i] = memory->table.pml4->entries[i];
    }

    for (uint64_t i = 0; i < memory->map.length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(&memory->map, i);
        if (desc->VirtualStart < PML_HIGHER_HALF_START)
        {
            panic(NULL, "Memory descriptor %d has invalid virtual address 0x%016lx", i, desc->VirtualStart);
        }
        if (desc->PhysicalStart > PML_LOWER_HALF_END)
        {
            panic(NULL, "Memory descriptor %d has invalid physical address 0x%016lx", i, desc->PhysicalStart);
        }

        if (page_table_map(&kernelSpace.pageTable, (void*)desc->VirtualStart, (void*)desc->PhysicalStart,
                desc->NumberOfPages, PML_WRITE | PML_GLOBAL | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
        {
            panic(NULL, "Failed to map memory descriptor %d (phys=0x%016lx-0x%016lx virt=0x%016lx)", i,
                desc->PhysicalStart, desc->PhysicalStart + desc->NumberOfPages * PAGE_SIZE, desc->VirtualStart);
        }
    }

    LOG_INFO("kernel virt=[0x%016lx-0x%016lx] phys=[0x%016lx-0x%016lx]\n", kernel->virtStart,
        kernel->virtStart + kernel->size, kernel->physStart, kernel->physStart + kernel->size);
    if (page_table_map(&kernelSpace.pageTable, (void*)kernel->virtStart, (void*)kernel->physStart,
            BYTES_TO_PAGES(kernel->size), PML_WRITE | PML_GLOBAL | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
    {
        panic(NULL, "Failed to map kernel memory");
    }

    LOG_INFO("GOP    virt=[0x%016lx-0x%016lx] phys=[0x%016lx-0x%016lx]\n", gop->virtAddr, gop->virtAddr + gop->size,
        gop->physAddr, gop->physAddr + gop->size);
    if (page_table_map(&kernelSpace.pageTable, (void*)gop->virtAddr, (void*)gop->physAddr, BYTES_TO_PAGES(gop->size),
            PML_WRITE | PML_GLOBAL | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
    {
        panic(NULL, "Failed to map GOP memory");
    }

    vmm_cpu_init();

    LOG_INFO("loading kernel page table... ");
    space_load(&kernelSpace);
    LOG_INFO("done!\n");
}

void vmm_cpu_init(void)
{
    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);
}

void vmm_map_bootloader_lower_half(thread_t* bootThread)
{
    for (pml_index_t i = PML_INDEX_LOWER_HALF_MIN; i < PML_INDEX_LOWER_HALF_MAX; i++)
    {
        bootThread->process->space.pageTable.pml4->entries[i] = kernelSpace.pageTable.pml4->entries[i];
    }
}

void vmm_unmap_bootloader_lower_half(thread_t* bootThread)
{
    for (pml_index_t i = PML_INDEX_LOWER_HALF_MIN; i < PML_INDEX_LOWER_HALF_MAX; i++)
    {
        bootThread->process->space.pageTable.pml4->entries[i].raw = 0;
        kernelSpace.pageTable.pml4->entries[i].raw = 0;
    }
}

space_t* vmm_get_kernel_space(void)
{
    return &kernelSpace;
}

pml_flags_t vmm_prot_to_flags(prot_t prot)
{
    if (!(prot & PROT_READ))
    {
        return 0;
    }

    return (prot & PROT_WRITE ? PML_WRITE : 0) | PML_PRESENT;
}

void vmm_align_region(void** virtAddr, uint64_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uint64_t)*virtAddr - (uint64_t)aligned);
    *virtAddr = aligned;
}

void* vmm_alloc(space_t* space, void* virtAddr, uint64_t length, pml_flags_t flags)
{
    if (length == 0 || !(flags & PML_PRESENT))
    {
        errno = EINVAL;
        return NULL;
    }

    if (space == NULL)
    {
        space = vmm_get_kernel_space();
    }

    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, NULL, length, flags | PML_OWNED) == ERR)
    {
        return NULL;
    }

    if (!page_table_is_unmapped(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        page_table_unmap(&space->pageTable, mapping.virtAddr, mapping.pageAmount);
    }

    for (uint64_t i = 0; i < mapping.pageAmount; i++)
    {
        void* addr = (void*)((uint64_t)mapping.virtAddr + i * PAGE_SIZE);
        void* page = pmm_alloc();

        if (page == NULL || page_table_map(&space->pageTable, addr, page, 1, mapping.flags, PML_CALLBACK_NONE) == ERR)
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

void* vmm_map(space_t* space, void* virtAddr, void* physAddr, uint64_t length, pml_flags_t flags,
    space_callback_func_t func, void* private)
{
    if (physAddr == NULL || length == 0 || !(flags & PML_PRESENT))
    {
        errno = EINVAL;
        return NULL;
    }

    if (space == NULL)
    {
        space = vmm_get_kernel_space();
    }

    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, physAddr, length, flags) == ERR)
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

    if (!page_table_is_unmapped(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        page_table_unmap(&space->pageTable, mapping.virtAddr, mapping.pageAmount);
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

void* vmm_map_pages(space_t* space, void* virtAddr, void** pages, uint64_t pageAmount, pml_flags_t flags,
    space_callback_func_t func, void* private)
{
    if (pages == NULL || pageAmount == 0 || !(flags & PML_PRESENT))
    {
        errno = EINVAL;
        return NULL;
    }

    if (space == NULL)
    {
        space = vmm_get_kernel_space();
    }

    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, NULL, pageAmount * PAGE_SIZE, flags) == ERR)
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

    if (!page_table_is_unmapped(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        page_table_unmap(&space->pageTable, mapping.virtAddr, mapping.pageAmount);
    }

    for (uint64_t i = 0; i < mapping.pageAmount; i++)
    {
        if (page_table_map(&space->pageTable, (void*)((uint64_t)mapping.virtAddr + i * PAGE_SIZE), pages[i], 1,
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

uint64_t vmm_unmap(space_t* space, void* virtAddr, uint64_t length)
{
    if (virtAddr == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (space == NULL)
    {
        space = vmm_get_kernel_space();
    }

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    RWMUTEX_WRITE_SCOPE(&space->mutex);

    if (page_table_is_unmapped(&space->pageTable, virtAddr, pageAmount))
    {
        return 0;
    }

    // Stores the amount of pages that have each callback id within the region.
    uint64_t callbacks[PML_MAX_CALLBACK] = {0};
    page_table_collect_callbacks(&space->pageTable, virtAddr, pageAmount, callbacks);

    page_table_unmap(&space->pageTable, virtAddr, pageAmount);

    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        space_callback_t* callback = &space->callbacks[index];
        assert(callback->pageAmount >= callbacks[index]);

        callback->pageAmount -= callbacks[index];
        if (callback->pageAmount == 0)
        {
            space->callbacks[index].func(space->callbacks[index].private);
            space_free_callback(space, index);
        }
    }

    return 0;
}

SYSCALL_DEFINE(SYS_MUNMAP, uint64_t, void* address, uint64_t length)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    RWMUTEX_WRITE_SCOPE(&space->mutex);

    if (!syscall_is_pointer_valid(address, length))
    {
        errno = EFAULT;
        return ERR;
    }

    return vmm_unmap(space, address, length);
}

uint64_t vmm_protect(space_t* space, void* virtAddr, uint64_t length, pml_flags_t flags)
{
    if (space == NULL || virtAddr == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (!(flags & PML_PRESENT))
    {
        return vmm_unmap(space, virtAddr, length);
    }

    if (space == NULL)
    {
        space = vmm_get_kernel_space();
    }

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    RWMUTEX_WRITE_SCOPE(&space->mutex);

    if (page_table_is_unmapped(&space->pageTable, virtAddr, pageAmount))
    {
        errno = EFAULT;
        return ERR;
    }

    uint64_t result = page_table_set_flags(&space->pageTable, virtAddr, pageAmount, flags);
    if (result == ERR)
    {
        errno = ENOENT;
        return ERR;
    }

    return 0;
}

SYSCALL_DEFINE(SYS_MPROTECT, uint64_t, void* address, uint64_t length, prot_t prot)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    RWMUTEX_WRITE_SCOPE(&space->mutex);

    if (!syscall_is_pointer_valid(address, length))
    {
        errno = EFAULT;
        return ERR;
    }

    return vmm_protect(space, address, length, vmm_prot_to_flags(prot) | PML_USER);
}
