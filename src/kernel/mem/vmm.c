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

    if (IS_ERR(space_init(&kernelSpace, VMM_KERNEL_HEAP_MIN, VMM_KERNEL_HEAP_MAX, SPACE_USE_PMM_BITMAP)))
    {
        panic(NULL, "Failed to initialize kernel address space");
    }

    LOG_DEBUG("address space layout:\n");
    LOG_DEBUG("  kernel binary:    %p-%p\n", VMM_KERNEL_BINARY_MIN, VMM_KERNEL_BINARY_MAX);
    LOG_DEBUG("  kernel stacks:    %p-%p\n", VMM_KERNEL_STACKS_MIN, VMM_KERNEL_STACKS_MAX);
    LOG_DEBUG("  kernel heap:      %p-%p\n", VMM_KERNEL_HEAP_MIN, VMM_KERNEL_HEAP_MAX);
    LOG_DEBUG("  identity map:     %p-%p\n", VMM_IDENTITY_MAPPED_MIN, VMM_IDENTITY_MAPPED_MAX);
    LOG_DEBUG("  user space:       %p-%p\n", VMM_USER_SPACE_MIN, VMM_USER_SPACE_MAX);

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
            panic(NULL, "Memory descriptor %d has invalid virtual address %p", i, desc->VirtualStart);
        }
        if (desc->PhysicalStart > PML_LOWER_HALF_END)
        {
            panic(NULL, "Memory descriptor %d has invalid physical address %p", i, desc->PhysicalStart);
        }

        if (!page_table_map(&kernelSpace.pageTable, (void*)desc->VirtualStart, desc->PhysicalStart, desc->NumberOfPages,
                PML_WRITE | PML_GLOBAL | PML_PRESENT, PML_CALLBACK_NONE))
        {
            panic(NULL, "Failed to map memory descriptor %d (phys=%p-%p virt=%p)", i, desc->PhysicalStart,
                desc->PhysicalStart + desc->NumberOfPages * PAGE_SIZE, desc->VirtualStart);
        }
    }

    Elf64_Addr minVaddr = 0;
    Elf64_Addr maxVaddr = 0;
    elf64_get_loadable_bounds(&kernel->elf, &minVaddr, &maxVaddr);
    uint64_t kernelPageAmount = BYTES_TO_PAGES(maxVaddr - minVaddr);

    LOG_INFO("kernel virt=[%p-%p] phys=[%p-%p]\n", minVaddr, maxVaddr, (uintptr_t)kernel->physAddr,
        (uintptr_t)kernel->physAddr + kernelPageAmount * PAGE_SIZE);
    if (!page_table_map(&kernelSpace.pageTable, (void*)minVaddr, kernel->physAddr, kernelPageAmount,
            PML_WRITE | PML_PRESENT, PML_CALLBACK_NONE))
    {
        panic(NULL, "Failed to map kernel memory");
    }

    LOG_INFO("GOP    virt=[%p-%p] phys=[%p-%p]\n", gop->virtAddr, gop->virtAddr + gop->size, gop->physAddr,
        gop->physAddr + gop->size);
    if (!page_table_map(&kernelSpace.pageTable, (void*)gop->virtAddr, gop->physAddr, BYTES_TO_PAGES(gop->size),
            PML_WRITE | PML_GLOBAL | PML_PRESENT, PML_CALLBACK_NONE))
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

static pml_callback_id_t vmm_alloc_callback(space_t* space, size_t pageAmount, space_callback_func_t func, void* data)
{
    if (space == NULL)
    {
        return PML_MAX_CALLBACK;
    }

    pml_callback_id_t callbackId = bitmap_find_first_clear(&space->callbackBitmap, 0, PML_MAX_CALLBACK);
    if (callbackId == PML_MAX_CALLBACK)
    {
        return PML_MAX_CALLBACK;
    }

    if (callbackId >= space->callbacksLength)
    {
        space_callback_t* newCallbacks = malloc(sizeof(space_callback_t) * (callbackId + 1));
        if (newCallbacks == NULL)
        {
            return PML_MAX_CALLBACK;
        }

        if (space->callbacks != NULL)
        {
            memcpy(newCallbacks, space->callbacks, sizeof(space_callback_t) * space->callbacksLength);
            free(space->callbacks);
        }
        memset(&newCallbacks[space->callbacksLength], 0,
            sizeof(space_callback_t) * (callbackId + 1 - space->callbacksLength));

        space->callbacks = newCallbacks;
        space->callbacksLength = callbackId + 1;
    }

    bitmap_set(&space->callbackBitmap, callbackId);
    space_callback_t* callback = &space->callbacks[callbackId];
    callback->func = func;
    callback->data = data;
    callback->pageAmount = pageAmount;
    return callbackId;
}

static void vmm_free_callback(space_t* space, pml_callback_id_t callbackId)
{
    bitmap_clear(&space->callbackBitmap, callbackId);
}

static void* vmm_find_free_region(space_t* space, size_t pageAmount, size_t alignment)
{
    void* addr;
    if (page_table_find_unmapped_region(&space->pageTable, space->freeAddress, space->endAddress,
            pageAmount, alignment, &addr))
    {
        space->freeAddress = (uintptr_t)addr + (pageAmount * PAGE_SIZE);
        assert(page_table_is_unmapped(&space->pageTable, addr, pageAmount));
        return addr;
    }

    if (page_table_find_unmapped_region(&space->pageTable, space->startAddress, space->freeAddress,
            pageAmount, alignment, &addr))
    {
        assert(page_table_is_unmapped(&space->pageTable, addr, pageAmount));
        return addr;
    }

    return NULL;
}

static void vmm_align_region(void** addr, size_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*addr, PAGE_SIZE);
    *length += ((uintptr_t)*addr - (uintptr_t)aligned);
    *addr = aligned;
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

status_t vmm_alloc(space_t* space, void** addr, size_t length, size_t alignment, pml_flags_t pmlFlags,
    vmm_alloc_flags_t allocFlags)
{
    if (addr == NULL ||length == 0 || !(pmlFlags & PML_PRESENT))
    {
        return ERR(MMU, INVAL);
    }

    if (*addr + length > *addr)
    {
        return ERR(MMU, TOOBIG);
    }

    if (pmlFlags & PML_USER && (*addr < (void*)VMM_USER_SPACE_MIN || *addr + length > (void*)VMM_USER_SPACE_MAX))
    {
        return ERR(MMU, ACCESS);
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    LOCK_SCOPE(&space->lock);

    size_t pageAmount;
    if (*addr == NULL)
    {
        pageAmount = BYTES_TO_PAGES(length);
        *addr = vmm_find_free_region(space, pageAmount, alignment);
        if (*addr == NULL)
        {
            return ERR(MMU, NOSPACE);
        }
    }
    else
    {
        vmm_align_region(addr, &length);
        pageAmount = BYTES_TO_PAGES(length);

        if ((uintptr_t)addr % alignment != 0)
        {
            return ERR(MMU, ALIGN);
        }
    }

    if (page_table_is_pinned(&space->pageTable, *addr, pageAmount))
    {
        return ERR(MMU, PINNED);
    }

    if (!page_table_is_unmapped(&space->pageTable, *addr, pageAmount))
    {
        if (allocFlags & VMM_ALLOC_FAIL_IF_MAPPED)
        {
            return ERR(MMU, MAPPED);
        }

        vmm_page_table_unmap_with_shootdown(space, *addr, pageAmount);
    }

    const uint64_t maxBatchSize = 64;
    size_t remainingPages = pageAmount;
    while (remainingPages > 0)
    {
        void* current = *addr + ((pageAmount - remainingPages) * PAGE_SIZE);

        pfn_t pages[maxBatchSize];
        uint64_t batchSize = MIN(remainingPages, maxBatchSize);
        if (!pmm_alloc_pages(pages, batchSize))
        {
            // Page table will free the previously allocated pages as they are owned by the Page table.
            vmm_page_table_unmap_with_shootdown(space, *addr, pageAmount - remainingPages);
            return ERR(MMU, NOMEM);
        }

        if (allocFlags & VMM_ALLOC_ZERO)
        {
            for (uint64_t i = 0; i < batchSize; i++)
            {
                memset(PFN_TO_VIRT(pages[i]), 0, PAGE_SIZE);
            }
        }

        if (!page_table_map_pages(&space->pageTable, current, pages, batchSize, pmlFlags,
                PML_CALLBACK_NONE))
        {
            vmm_page_table_unmap_with_shootdown(space, *addr, pageAmount - remainingPages);
            return ERR(MMU, NOMEM);
        }

        remainingPages -= batchSize;
    }

    return OK;
}

status_t vmm_map(space_t* space, void** addr, phys_addr_t phys, size_t length, pml_flags_t flags,
    space_callback_func_t func, void* data)
{
    if (addr == NULL || phys == PHYS_ADDR_INVALID || length == 0 || !(flags & PML_PRESENT))
    {
        return ERR(MMU, INVAL);
    }

    if (*addr + length < *addr)
    {
        return ERR(MMU, TOOBIG);
    }

    if (phys + length < phys)
    {
        return ERR(MMU, TOOBIG);
    }

    if (flags & PML_USER && (*addr < (void*)VMM_USER_SPACE_MIN || *addr + length > (void*)VMM_USER_SPACE_MAX))
    {
        return ERR(MMU, ACCESS);
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    LOCK_SCOPE(&space->lock);

    size_t pageAmount;
    if (*addr == NULL)
    {
        pageAmount = BYTES_TO_PAGES(length);
        *addr = vmm_find_free_region(space, pageAmount, 1);
        if (*addr == NULL)
        {
            return ERR(MMU, NOSPACE);
        }
    }
    else
    {
        vmm_align_region(addr, &length);
        pageAmount = BYTES_TO_PAGES(length);
    }

    if (page_table_is_pinned(&space->pageTable, *addr, pageAmount))
    {
        return ERR(MMU, PINNED);
    }

    pml_callback_id_t callbackId = PML_CALLBACK_NONE;
    if (func != NULL)
    {
        callbackId = vmm_alloc_callback(space, pageAmount, func, data);
        if (callbackId == PML_MAX_CALLBACK)
        {
            return ERR(MMU, SHARED_LIMIT);
        }
    }

    if (!page_table_is_unmapped(&space->pageTable, *addr, pageAmount))
    {
        vmm_page_table_unmap_with_shootdown(space, *addr, pageAmount);
    }

    if (!page_table_map(&space->pageTable, *addr, PML_ENSURE_LOWER_HALF(ROUND_DOWN(phys, PAGE_SIZE)), pageAmount, flags,
            callbackId))
    {
        if (callbackId != PML_CALLBACK_NONE)
        {
            vmm_free_callback(space, callbackId);
        }

        vmm_page_table_unmap_with_shootdown(space, *addr, pageAmount);
        return ERR(MMU, NOMEM);
    }

    return OK;
}

status_t vmm_map_pages(space_t* space, void** addr, pfn_t* pfns, size_t amount, pml_flags_t flags,
    space_callback_func_t func, void* data)
{
    if (addr == NULL || pfns == NULL || amount == 0 || !(flags & PML_PRESENT))
    {
        return ERR(MMU, INVAL);
    }

    if (*addr + amount * PAGE_SIZE < *addr)
    {
        return ERR(MMU, TOOBIG);
    }

    if (flags & PML_USER &&
        (*addr < (void*)VMM_USER_SPACE_MIN || *addr + amount * PAGE_SIZE > (void*)VMM_USER_SPACE_MAX))
    {
        return ERR(MMU, ACCESS);
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    LOCK_SCOPE(&space->lock);

    if (*addr == NULL)
    {
        *addr = vmm_find_free_region(space, amount, 1);
        if (*addr == NULL)
        {
            return ERR(MMU, NOSPACE);
        }
    }
    else
    {
        size_t length = amount * PAGE_SIZE;
        vmm_align_region(addr, &length);
        amount = BYTES_TO_PAGES(length);
    }

    if (page_table_is_pinned(&space->pageTable, *addr, amount))
    {
        return ERR(MMU, PINNED);
    }

    pml_callback_id_t callbackId = PML_CALLBACK_NONE;
    if (func != NULL)
    {
        callbackId = vmm_alloc_callback(space, amount, func, data);
        if (callbackId == PML_MAX_CALLBACK)
        {
            return ERR(MMU, SHARED_LIMIT);
        }
    }

    if (!page_table_is_unmapped(&space->pageTable, *addr, amount))
    {
        vmm_page_table_unmap_with_shootdown(space, *addr, amount);
    }

    if (!page_table_map_pages(&space->pageTable, *addr, pfns, amount, flags, callbackId))
    {
        if (callbackId != PML_CALLBACK_NONE)
        {
            vmm_free_callback(space, callbackId);
        }

        vmm_page_table_unmap_with_shootdown(space, *addr, amount);
        return ERR(MMU, NOMEM);
    }

    return OK;
}

status_t vmm_unmap(space_t* space, void* virtAddr, size_t length)
{
    if (virtAddr == NULL || length == 0)
    {
        return ERR(MMU, INVAL);
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    LOCK_SCOPE(&space->lock);

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    if (page_table_is_pinned(&space->pageTable, virtAddr, pageAmount))
    {
        return ERR(MMU, PINNED);
    }

    // Stores the amount of pages that have each callback id within the region.
    uint64_t callbacks[PML_MAX_CALLBACK] = {0};
    page_table_collect_callbacks(&space->pageTable, virtAddr, pageAmount, callbacks);

    vmm_page_table_unmap_with_shootdown(space, virtAddr, pageAmount);

    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        space_callback_t* callback = &space->callbacks[index];
        if (callback->pageAmount >= callbacks[index])
        {
            callback->pageAmount -= callbacks[index];
        }
        else
        {
            callback->pageAmount = 0;
        }

        if (callback->pageAmount == 0)
        {
            assert(index < space->callbacksLength);
            space->callbacks[index].func(space->callbacks[index].data);
            vmm_free_callback(space, index);
        }
    }

    return OK;
}

SYSCALL_DEFINE(SYS_MUNMAP, void* address, size_t length)
{
    process_t* process = process_current();
    space_t* space = &process->space;

    if (!space_check_access(space, address, length))
    {
        return ERR(MMU, FAULT);
    }

    return vmm_unmap(space, address, length);
}

status_t vmm_protect(space_t* space, void* virtAddr, size_t length, pml_flags_t flags)
{
    if (space == NULL || virtAddr == NULL || length == 0)
    {
        return ERR(MMU, INVAL);
    }

    if (!(flags & PML_PRESENT))
    {
        return vmm_unmap(space, virtAddr, length);
    }

    if (space == NULL)
    {
        space = vmm_kernel_space_get();
    }

    LOCK_SCOPE(&space->lock);

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    if (page_table_is_pinned(&space->pageTable, virtAddr, pageAmount))
    {
        return ERR(MMU, PINNED);
    }

    if (page_table_is_unmapped(&space->pageTable, virtAddr, pageAmount))
    {
        return ERR(MMU, UNMAPPED);
    }

    if (!page_table_set_flags(&space->pageTable, virtAddr, pageAmount, flags))
    {
        return ERR(MMU, INVAL);
    }

    vmm_tlb_shootdown(space, virtAddr, pageAmount);

    return OK;
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

        if (ipi_send(cpu_get_by_id(id), IPI_SINGLE, vmm_tlb_shootdown_ipi, NULL) == _FAIL)
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
                (void*)((uintptr_t)virtAddr + (pageAmount * PAGE_SIZE)));
        }

        ASM("pause");
    }
}

SYSCALL_DEFINE(SYS_MPROTECT, void* address, size_t length, prot_t prot)
{
    process_t* process = process_current();
    space_t* space = &process->space;

    if (!space_check_access(space, address, length))
    {
        return ERR(MMU, FAULT);
    }

    return vmm_protect(space, address, length, vmm_prot_to_flags(prot));
}