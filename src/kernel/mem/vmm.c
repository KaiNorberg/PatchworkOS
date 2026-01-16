#include <kernel/mem/vmm.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/regs.h>
#include <kernel/cpu/syscall.h>
#include <kernel/init/boot_info.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging.h>
#include <kernel/mem/pmm.h>
#include <kernel/mem/space.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>

#include <boot/boot_info.h>

#include <assert.h>
#include <errno.h>
#include <sys/math.h>
#include <sys/proc.h>

static space_t kernelSpace;

static void vmm_cpu_init(vmm_cpu_t* ctx)
{
    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);

    ctx->shootdownCount = 0;
    lock_init(&ctx->lock);

    cr3_write(PML_ENSURE_LOWER_HALF(kernelSpace.pageTable.pml4));
    ctx->space = &kernelSpace;
    lock_acquire(&kernelSpace.lock);
    bitmap_set(&kernelSpace.cpus, SELF->id);
    lock_release(&kernelSpace.lock);
}

PERCPU_DEFINE_CTOR(static vmm_cpu_t, pcpu_vmm)
{
    vmm_cpu_init(SELF_PTR(pcpu_vmm));
}

void vmm_init(void)
{
    boot_info_t* bootInfo = boot_info_get();
    const boot_memory_t* memory = &bootInfo->memory;
    const boot_gop_t* gop = &bootInfo->gop;
    const boot_kernel_t* kernel = &bootInfo->kernel;

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

    for (size_t i = 0; i < memory->map.length; i++)
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

    Elf64_Addr minVaddr = 0;
    Elf64_Addr maxVaddr = 0;
    elf64_get_loadable_bounds(&kernel->elf, &minVaddr, &maxVaddr);
    uint64_t kernelPageAmount = BYTES_TO_PAGES(maxVaddr - minVaddr);

    LOG_INFO("kernel virt=[0x%016lx-0x%016lx] phys=[0x%016lx-0x%016lx]\n", minVaddr, maxVaddr,
        (uintptr_t)kernel->physAddr, (uintptr_t)kernel->physAddr + kernelPageAmount * PAGE_SIZE);
    if (page_table_map(&kernelSpace.pageTable, (void*)minVaddr, kernel->physAddr, kernelPageAmount,
            PML_WRITE | PML_PRESENT, PML_CALLBACK_NONE) == ERR)
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
}

void vmm_kernel_space_load(void)
{
    LOG_INFO("loading kernel space... ");
    vmm_cpu_init(SELF_PTR(pcpu_vmm));
    LOG_INFO("done!\n");
}

space_t* vmm_kernel_space_get(void)
{
    return &kernelSpace;
}

pml_flags_t vmm_prot_to_flags(prot_t prot)
{
    switch ((int)prot)
    {
    case PROT_NONE:
        return 0;
    case PROT_READ:
        return PML_PRESENT;
    case PROT_READ | PROT_WRITE:
        return PML_PRESENT | PML_WRITE;
    default:
        return 0;
    }
}

// Handles the logic of unmapping with a shootdown, should be called with the spaces lock acquired.
// We need to make sure that any underlying physical pages owned by the page table are freed after every CPU
// has invalidated their TLBs.
static inline void vmm_page_table_unmap_with_shootdown(space_t* space, void* virtAddr, uint64_t pageAmount)
{
    page_table_unmap(&space->pageTable, virtAddr, pageAmount);
    vmm_tlb_shootdown(space, virtAddr, pageAmount);
    page_table_clear(&space->pageTable, virtAddr, pageAmount);
}

void* vmm_alloc(space_t* space, void* virtAddr, size_t length, size_t alignment, pml_flags_t pmlFlags,
    vmm_alloc_flags_t allocFlags)
{
    if (length == 0 || !(pmlFlags & PML_PRESENT))
    {
        errno = EINVAL;
        return NULL;
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, NULL, length, alignment, pmlFlags | PML_OWNED) == ERR)
    {
        return NULL;
    }

    if (page_table_is_pinned(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        return space_mapping_end(space, &mapping, EBUSY);
    }

    if (!page_table_is_unmapped(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        if (allocFlags & VMM_ALLOC_FAIL_IF_MAPPED)
        {
            return space_mapping_end(space, &mapping, EEXIST);
        }

        vmm_page_table_unmap_with_shootdown(space, mapping.virtAddr, mapping.pageAmount);
    }

    const uint64_t maxBatchSize = 64;
    uint64_t remainingPages = mapping.pageAmount;
    while (remainingPages != 0)
    {
        uintptr_t currentVirtAddr = (uintptr_t)mapping.virtAddr + (mapping.pageAmount - remainingPages) * PAGE_SIZE;

        void* addresses[maxBatchSize];
        uint64_t batchSize = MIN(remainingPages, maxBatchSize);
        if (pmm_alloc_pages(addresses, batchSize) == ERR)
        {
            // Page table will free the previously allocated pages as they are owned by the Page table.
            vmm_page_table_unmap_with_shootdown(space, mapping.virtAddr, mapping.pageAmount - remainingPages);
            return space_mapping_end(space, &mapping, ENOMEM);
        }

        if (allocFlags & VMM_ALLOC_ZERO)
        {
            for (uint64_t i = 0; i < batchSize; i++)
            {
                memset(addresses[i], 0, PAGE_SIZE);
            }
        }

        if (page_table_map_pages(&space->pageTable, (void*)currentVirtAddr, addresses, batchSize, mapping.flags,
                PML_CALLBACK_NONE) == ERR)
        {
            // Page table will free the previously allocated pages as they are owned by the Page table.
            vmm_page_table_unmap_with_shootdown(space, mapping.virtAddr, mapping.pageAmount - remainingPages);
            return space_mapping_end(space, &mapping, ENOMEM);
        }

        remainingPages -= batchSize;
    }

    return space_mapping_end(space, &mapping, EOK);
}

void* vmm_map(space_t* space, void* virtAddr, void* physAddr, size_t length, pml_flags_t flags,
    space_callback_func_t func, void* data)
{
    if (physAddr == NULL || length == 0 || !(flags & PML_PRESENT))
    {
        errno = EINVAL;
        return NULL;
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, physAddr, length, 1, flags) == ERR)
    {
        return NULL;
    }

    if (page_table_is_pinned(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        return space_mapping_end(space, &mapping, EBUSY);
    }

    pml_callback_id_t callbackId = PML_CALLBACK_NONE;
    if (func != NULL)
    {
        callbackId = space_alloc_callback(space, mapping.pageAmount, func, data);
        if (callbackId == PML_MAX_CALLBACK)
        {
            return space_mapping_end(space, &mapping, ENOSPC);
        }
    }

    if (!page_table_is_unmapped(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        vmm_page_table_unmap_with_shootdown(space, mapping.virtAddr, mapping.pageAmount);
    }

    if (page_table_map(&space->pageTable, mapping.virtAddr, mapping.physAddr, mapping.pageAmount, flags, callbackId) ==
        ERR)
    {
        if (callbackId != PML_CALLBACK_NONE)
        {
            space_free_callback(space, callbackId);
        }

        vmm_page_table_unmap_with_shootdown(space, mapping.virtAddr, mapping.pageAmount);
        return space_mapping_end(space, &mapping, ENOMEM);
    }

    return space_mapping_end(space, &mapping, EOK);
}

void* vmm_map_pages(space_t* space, void* virtAddr, void** pages, size_t pageAmount, pml_flags_t flags,
    space_callback_func_t func, void* data)
{
    if (pages == NULL || pageAmount == 0 || !(flags & PML_PRESENT))
    {
        errno = EINVAL;
        return NULL;
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, NULL, pageAmount * PAGE_SIZE, 1, flags) == ERR)
    {
        return NULL;
    }

    if (page_table_is_pinned(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        return space_mapping_end(space, &mapping, EBUSY);
    }

    pml_callback_id_t callbackId = PML_CALLBACK_NONE;
    if (func != NULL)
    {
        callbackId = space_alloc_callback(space, pageAmount, func, data);
        if (callbackId == PML_MAX_CALLBACK)
        {
            return space_mapping_end(space, &mapping, ENOSPC);
        }
    }

    if (!page_table_is_unmapped(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        vmm_page_table_unmap_with_shootdown(space, mapping.virtAddr, mapping.pageAmount);
    }

    if (page_table_map_pages(&space->pageTable, mapping.virtAddr, pages, mapping.pageAmount, mapping.flags,
            callbackId) == ERR)
    {
        if (callbackId != PML_CALLBACK_NONE)
        {
            space_free_callback(space, callbackId);
        }

        vmm_page_table_unmap_with_shootdown(space, mapping.virtAddr, mapping.pageAmount);
        return space_mapping_end(space, &mapping, ENOMEM);
    }

    return space_mapping_end(space, &mapping, EOK);
}

void* vmm_unmap(space_t* space, void* virtAddr, size_t length)
{
    if (virtAddr == NULL || length == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, NULL, length, 1, PML_NONE) == ERR)
    {
        return NULL;
    }

    if (page_table_is_pinned(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        return space_mapping_end(space, &mapping, EBUSY);
    }

    // Stores the amount of pages that have each callback id within the region.
    uint64_t callbacks[PML_MAX_CALLBACK] = {0};
    page_table_collect_callbacks(&space->pageTable, mapping.virtAddr, mapping.pageAmount, callbacks);

    vmm_page_table_unmap_with_shootdown(space, mapping.virtAddr, mapping.pageAmount);

    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        space_callback_t* callback = &space->callbacks[index];
        assert(callback->pageAmount >= callbacks[index]);

        callback->pageAmount -= callbacks[index];
        if (callback->pageAmount == 0)
        {
            assert(index < space->callbacksLength);
            space->callbacks[index].func(space->callbacks[index].data);
            space_free_callback(space, index);
        }
    }

    return space_mapping_end(space, &mapping, EOK);
}

SYSCALL_DEFINE(SYS_MUNMAP, void*, void* address, size_t length)
{
    process_t* process = process_current();
    space_t* space = &process->space;

    if (space_check_access(space, address, length) == ERR)
    {
        errno = EFAULT;
        return NULL;
    }

    return vmm_unmap(space, address, length);
}

void* vmm_protect(space_t* space, void* virtAddr, size_t length, pml_flags_t flags)
{
    if (space == NULL || virtAddr == NULL || length == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    if (!(flags & PML_PRESENT))
    {
        return vmm_unmap(space, virtAddr, length);
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    space_mapping_t mapping;
    if (space_mapping_start(space, &mapping, virtAddr, NULL, length, 1, flags) == ERR)
    {
        return NULL;
    }

    if (page_table_is_pinned(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        return space_mapping_end(space, &mapping, EBUSY);
    }

    if (page_table_is_unmapped(&space->pageTable, mapping.virtAddr, mapping.pageAmount))
    {
        return space_mapping_end(space, &mapping, ENOENT);
    }

    if (page_table_set_flags(&space->pageTable, mapping.virtAddr, mapping.pageAmount, mapping.flags) == ERR)
    {
        return space_mapping_end(space, &mapping, EINVAL);
    }

    vmm_tlb_shootdown(space, mapping.virtAddr, mapping.pageAmount);

    return space_mapping_end(space, &mapping, EOK);
}

void vmm_load(space_t* space)
{
    if (space == NULL)
    {
        return;
    }

    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    assert(pcpu_vmm->space != NULL);
    if (space == pcpu_vmm->space)
    {
        return;
    }

    space_t* oldSpace = pcpu_vmm->space;
    pcpu_vmm->space = NULL;

    lock_acquire(&oldSpace->lock);
    bitmap_clear(&oldSpace->cpus, SELF->id);
    lock_release(&oldSpace->lock);

    lock_acquire(&space->lock);
    bitmap_set(&space->cpus, SELF->id);
    lock_release(&space->lock);
    pcpu_vmm->space = space;

    page_table_load(&space->pageTable);
}

static void vmm_tlb_shootdown_ipi(ipi_func_data_t* data)
{
    UNUSED(data);

    vmm_cpu_t* vmm = SELF_PTR(pcpu_vmm);
    while (true)
    {
        lock_acquire(&vmm->lock);
        if (vmm->shootdownCount == 0)
        {
            lock_release(&vmm->lock);
            break;
        }

        vmm_shootdown_t shootdown = vmm->shootdowns[vmm->shootdownCount - 1];
        vmm->shootdownCount--;
        lock_release(&vmm->lock);

        assert(shootdown.space != NULL);
        assert(shootdown.pageAmount != 0);
        assert(shootdown.virtAddr != NULL);

        tlb_invalidate(shootdown.virtAddr, shootdown.pageAmount);
        atomic_fetch_add(&shootdown.space->shootdownAcks, 1);
    }
}

void vmm_tlb_shootdown(space_t* space, void* virtAddr, size_t pageAmount)
{
    if (space == NULL)
    {
        return;
    }

    if (cpu_amount() <= 1)
    {
        return;
    }

    uint16_t expectedAcks = 0;
    atomic_store(&space->shootdownAcks, 0);

    cpu_id_t id;
    BITMAP_FOR_EACH_SET(&id, &space->cpus)
    {
        if (id == SELF->id)
        {
            continue;
        }

        vmm_cpu_t* cpu = CPU_PTR(id, pcpu_vmm);

        lock_acquire(&cpu->lock);
        if (cpu->shootdownCount >= VMM_MAX_SHOOTDOWN_REQUESTS)
        {
            lock_release(&cpu->lock);
            panic(NULL, "CPU %d shootdown buffer overflow", id);
        }

        vmm_shootdown_t* shootdown = &cpu->shootdowns[cpu->shootdownCount++];
        shootdown->space = space;
        shootdown->virtAddr = virtAddr;
        shootdown->pageAmount = pageAmount;
        lock_release(&cpu->lock);

        if (ipi_send(cpu_get_by_id(id), IPI_SINGLE, vmm_tlb_shootdown_ipi, NULL) == ERR)
        {
            panic(NULL, "Failed to send TLB shootdown IPI to CPU %d", id);
        }
        expectedAcks++;
    }

    clock_t startTime = clock_uptime();
    while (atomic_load(&space->shootdownAcks) < expectedAcks)
    {
        if (clock_uptime() - startTime > SPACE_TLB_SHOOTDOWN_TIMEOUT)
        {
            panic(NULL, "TLB shootdown timeout in space %p for region %p - %p", space, virtAddr,
                (void*)((uintptr_t)virtAddr + pageAmount * PAGE_SIZE));
        }

        ASM("pause");
    }
}

SYSCALL_DEFINE(SYS_MPROTECT, void*, void* address, size_t length, prot_t prot)
{
    process_t* process = process_current();
    space_t* space = &process->space;

    if (space_check_access(space, address, length) == ERR)
    {
        return NULL;
    }

    return vmm_protect(space, address, length, vmm_prot_to_flags(prot) | PML_USER);
}