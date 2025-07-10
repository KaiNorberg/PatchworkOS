#include "vmm.h"
#include "cpu/regs.h"
#include "log/log.h"
#include "log/panic.h"
#include "pml.h"
#include "pmm.h"
#include "sched/thread.h"
#include "sync/lock.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/math.h>

extern uint64_t _kernelEnd;

static lock_t kernelLock;
static pml_t* kernelPml;
static uintptr_t kernelFreeAddress;

static void* vmm_kernel_find_free_region(uint64_t pageAmount)
{
    uintptr_t addr = kernelFreeAddress;

    while (addr + pageAmount * PAGE_SIZE <= PML_HIGHER_HALF_END)
    {
        void* firstMappedPage =
            pml_find_first_mapped_page(kernelPml, (void*)addr, (void*)(addr + pageAmount * PAGE_SIZE));

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

void space_init(space_t* space)
{
    space->pml = pml_new();
    space->freeAddress = PML_LOWER_HALF_START;
    memset(space->callbacks, 0, sizeof(vmm_callback_t) * PML_MAX_CALLBACK);
    memset(space->bitmapBuffer, 0, BITMAP_BITS_TO_BYTES(PML_MAX_CALLBACK));
    bitmap_init(&space->callbackBitmap, space->bitmapBuffer, PML_MAX_CALLBACK);
    lock_init(&space->lock);

    LOCK_DEFER(&kernelLock);
    pml_t* kernelPml = vmm_kernel_pml();
    for (uint64_t i = PML_ENTRY_AMOUNT / 2; i < PML_ENTRY_AMOUNT; i++)
    {
        space->pml->entries[i] = kernelPml->entries[i];
    }
}

void space_deinit(space_t* space)
{
    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        space->callbacks[index].func(space->callbacks[index].private);
    }

    for (uint64_t i = PML_ENTRY_AMOUNT / 2; i < PML_ENTRY_AMOUNT; i++)
    {
        space->pml->entries[i] = (pml_entry_t){0};
    }

    pml_free(space->pml);
}

void space_load(space_t* space)
{
    if (space != NULL)
    {
        pml_load(space->pml);
    }
    else
    {
        pml_load(kernelPml);
    }
}

void space_update_free_address(space_t* space, uintptr_t virtAddr, uint64_t pageAmount)
{
    if (virtAddr <= space->freeAddress && space->freeAddress < virtAddr + pageAmount * PAGE_SIZE)
    {
        space->freeAddress = virtAddr + pageAmount * PAGE_SIZE;
    }
}

static pml_callback_id_t space_add_callback(space_t* space, uint64_t pageAmount, vmm_callback_func_t func,
    void* private)
{
    pml_callback_id_t callbackId = bitmap_find_first_clear(&space->callbackBitmap);
    if (callbackId == PML_MAX_CALLBACK)
    {
        return PML_MAX_CALLBACK;
    }

    bitmap_set(&space->callbackBitmap, callbackId, callbackId + 1);
    vmm_callback_t* callback = &space->callbacks[callbackId];
    callback->func = func;
    callback->private = private;
    callback->pageAmount = pageAmount;
    return callbackId;
}

static void space_remove_callback(space_t* space, uint8_t callbackId)
{
    bitmap_clear(&space->callbackBitmap, callbackId, callbackId + 1);
}

static void* vmm_find_free_region(space_t* space, uint64_t pageAmount)
{
    uintptr_t addr = space->freeAddress;
    while (addr < PML_LOWER_HALF_END)
    {
        void* firstMappedPage =
            pml_find_first_mapped_page(space->pml, (void*)addr, (void*)(addr + pageAmount * PAGE_SIZE));
        if (firstMappedPage != NULL)
        {
            addr = (uintptr_t)firstMappedPage + PAGE_SIZE;
            continue;
        }

        space->freeAddress = addr + pageAmount * PAGE_SIZE;
        return (void*)addr;
    }

    return NULL;
}

static void vmm_align_region(void** virtAddr, uint64_t* length)
{
    void* aligned = (void*)ROUND_DOWN(*virtAddr, PAGE_SIZE);
    *length += ((uint64_t)*virtAddr - (uint64_t)aligned);
    *virtAddr = aligned;
}

static uint64_t vmm_prot_to_flags(prot_t prot, pml_flags_t* flags)
{
    if (!(prot & PROT_READ))
    {
        return ERR;
    }
    *flags = (prot & PROT_WRITE ? PML_WRITE : 0) | PML_USER;
    return 0;
}

typedef struct
{
    void* virtAddr;
    void* physAddr;
    uint64_t pageAmount;
    pml_flags_t flags;
} vmm_mapping_info_t;

static uint64_t vmm_mapping_prepare(vmm_mapping_info_t* info, space_t* space, void* virtAddr, void* physAddr,
    uint64_t length, prot_t prot)
{
    if (length == 0)
    {
        return ERR;
    }

    if (vmm_prot_to_flags(prot, &info->flags) == ERR)
    {
        return ERR;
    }

    uint64_t pageAmount;
    if (virtAddr == NULL)
    {
        pageAmount = BYTES_TO_PAGES(length);
        virtAddr = vmm_find_free_region(space, pageAmount);
        if (virtAddr == NULL)
        {
            return ERR;
        }
    }
    else
    {
        vmm_align_region(&virtAddr, &length);
        pageAmount = BYTES_TO_PAGES(length);
    }

    info->virtAddr = virtAddr;
    if (info->physAddr != NULL)
    {
        info->physAddr = PML_ENSURE_LOWER_HALF((void*)ROUND_DOWN(physAddr, PAGE_SIZE));
    }
    else
    {
        info->physAddr = NULL;
    }
    info->pageAmount = pageAmount;
    return 0;
}

static void vmm_load_memory_map(efi_mem_map_t* memoryMap)
{
    // Kernel pml must be within 32 bit boundary because smp trampoline loads it as a dword.
    kernelPml = pmm_alloc_bitmap(1, UINT32_MAX, 0);
    if (kernelPml == NULL)
    {
        panic(NULL, "Failed to allocate kernel PML");
    }
    memset(kernelPml, 0, PAGE_SIZE);

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        uint64_t result = pml_map(kernelPml, desc->virtualStart, desc->physicalStart, desc->amountOfPages,
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

    LOG_INFO("vmm: kernel phys=[0x%016lx-0x%016lx] virt=[0x%016lx-0x%016lx]\n", kernel->physStart,
        kernel->physStart + kernel->length, kernel->virtStart, kernel->virtStart + kernel->length);

    uint64_t result = pml_map(kernelPml, kernel->virtStart, kernel->physStart, BYTES_TO_PAGES(kernel->length),
        PML_WRITE | VMM_KERNEL_PML_FLAGS, PML_CALLBACK_NONE);
    if (result == ERR)
    {
        panic(NULL, "Failed to map kernel (phys=0x%016lx-0x%016lx virt=0x%016lx)", kernel->physStart,
            kernel->physStart + kernel->length, kernel->virtStart);
    }

    LOG_INFO("vmm: loading kernel pml 0x%016lx\n", kernelPml);
    pml_load(kernelPml);
    LOG_INFO("vmm: kernel pml loaded\n");

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
    LOG_INFO("vmm: global page enable\n");
    cr4_write(cr4_read() | CR4_PAGE_GLOBAL_ENABLE);
}

pml_t* vmm_kernel_pml(void)
{
    return kernelPml;
}

void* vmm_kernel_map(void* virtAddr, void* physAddr, uint64_t pageAmount, pml_flags_t flags)
{
    LOCK_DEFER(&kernelLock);

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
            if (!pml_is_unmapped(kernelPml, virtAddr, pageAmount))
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
                pml_map(kernelPml, vaddr, PML_HIGHER_TO_LOWER(page), 1, flags | VMM_KERNEL_PML_FLAGS | PML_OWNED,
                    PML_CALLBACK_NONE) == ERR)
            {
                if (page != NULL)
                {
                    pmm_free(page);
                }

                // Page table will free the previously allocated pages as they are owned by the Page table.
                pml_unmap(kernelPml, virtAddr, i);
                LOG_WARN("vmm: failed to map kernel page at virt=0x%016lx (attempt %llu/%llu)\n", (uintptr_t)vaddr,
                    i + 1, pageAmount);
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
            LOG_DEBUG("vmm: map lower [0x%016lx-0x%016lx] to higher\n", physAddr,
                (uintptr_t)physAddr + pageAmount * PAGE_SIZE);
        }

        if (!pml_is_unmapped(kernelPml, virtAddr, pageAmount))
        {
            errno = EEXIST;
            return NULL;
        }

        if (pml_map(kernelPml, virtAddr, physAddr, pageAmount, flags | VMM_KERNEL_PML_FLAGS, PML_CALLBACK_NONE) == ERR)
        {
            errno = ENOMEM;
            return NULL;
        }
    }

    return virtAddr;
}

void vmm_kernel_unmap(void* virtAddr, uint64_t pageAmount)
{
    LOCK_DEFER(&kernelLock);

    // Sanity checks, if the kernel specifies weird values we assume some corruption or fundamental failure has taken
    // place.
    assert((uintptr_t)virtAddr % 0x1000 == 0);
    assert(pageAmount != 0);

    pml_unmap(kernelPml, virtAddr, pageAmount);
}

void* vmm_alloc(space_t* space, void* virtAddr, uint64_t length, prot_t prot)
{
    LOCK_DEFER(&space->lock);

    vmm_mapping_info_t info;
    if (vmm_mapping_prepare(&info, space, virtAddr, NULL, length, prot) == ERR)
    {
        errno = EINVAL;
        return NULL;
    }

    for (uint64_t i = 0; i < info.pageAmount; i++)
    {
        void* addr = (void*)((uint64_t)info.virtAddr + i * PAGE_SIZE);
        void* page = pmm_alloc();

        if (page == NULL ||
            pml_map(space->pml, addr, PML_HIGHER_TO_LOWER(page), 1, info.flags | PML_OWNED, PML_CALLBACK_NONE) == ERR)
        {
            if (page != NULL)
            {
                pmm_free(page);
            }

            // Page table will free the previously allocated pages as they are owned by the Page table.
            pml_unmap(space->pml, info.virtAddr, i);
            LOG_WARN("vmm: failed to map user page at virt=0x%016lx (attempt %llu/%llu)\n", (uintptr_t)addr, i + 1,
                info.pageAmount);
            errno = ENOMEM;
            return NULL;
        }
    }

    space_update_free_address(space, (uintptr_t)info.virtAddr, info.pageAmount);

    return info.virtAddr;
}

void* vmm_map(space_t* space, void* virtAddr, void* physAddr, uint64_t length, prot_t prot, vmm_callback_func_t func,
    void* private)
{
    LOCK_DEFER(&space->lock);

    vmm_mapping_info_t info;
    if (vmm_mapping_prepare(&info, space, virtAddr, physAddr, length, prot) == ERR)
    {
        errno = EINVAL;
        return NULL;
    }

    pml_callback_id_t callbackId = PML_CALLBACK_NONE;
    if (func != NULL)
    {
        callbackId = space_add_callback(space, info.pageAmount, func, private);
        if (callbackId == PML_MAX_CALLBACK)
        {
            errno = ENOSPC;
            return NULL;
        }
    }

    if (pml_map(space->pml, info.virtAddr, info.physAddr, info.pageAmount, info.flags, callbackId) == ERR)
    {
        if (callbackId != PML_CALLBACK_NONE)
        {
            space_remove_callback(space, callbackId);
        }
        LOG_WARN("vmm: failed to map user range virt=0x%016lx phys=0x%016lx pages=%llu\n", (uintptr_t)info.virtAddr,
            (uintptr_t)info.physAddr, info.pageAmount);
        errno = ENOMEM;
        return NULL;
    }

    space_update_free_address(space, (uintptr_t)info.virtAddr, info.pageAmount);

    return info.virtAddr;
}

void* vmm_map_pages(space_t* space, void* virtAddr, void** pages, uint64_t pageAmount, prot_t prot,
    vmm_callback_func_t func, void* private)
{
    LOCK_DEFER(&space->lock);

    vmm_mapping_info_t info;
    if (vmm_mapping_prepare(&info, space, virtAddr, NULL, pageAmount * PAGE_SIZE, prot) == ERR)
    {
        errno = EINVAL;
        return NULL;
    }

    pml_callback_id_t callbackId = PML_CALLBACK_NONE;
    if (func != NULL)
    {
        callbackId = space_add_callback(space, pageAmount, func, private);
        if (callbackId == PML_MAX_CALLBACK)
        {
            errno = ENOSPC;
            return NULL;
        }
    }

    for (uint64_t i = 0; i < info.pageAmount; i++)
    {
        void* physAddr = PML_ENSURE_LOWER_HALF(pages[i]);

        if (pml_map(space->pml, (void*)((uint64_t)info.virtAddr + i * PAGE_SIZE), physAddr, 1, info.flags,
                callbackId) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                pml_unmap(space->pml, (void*)((uint64_t)info.virtAddr + j * PAGE_SIZE), 1);
            }

            if (callbackId != PML_CALLBACK_NONE)
            {
                space_remove_callback(space, callbackId);
            }

            LOG_WARN("vmm: failed to map user page at virt=0x%016lx phys=0x%016lx (attempt %llu/%llu)\n",
                (uintptr_t)info.virtAddr + i * PAGE_SIZE, (uintptr_t)physAddr, i + 1, info.pageAmount);
            errno = ENOMEM;
            return NULL;
        }
    }

    space_update_free_address(space, (uintptr_t)info.virtAddr, info.pageAmount);

    return info.virtAddr;
}

uint64_t vmm_unmap(space_t* space, void* virtAddr, uint64_t length)
{
    LOCK_DEFER(&space->lock);

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    if (pml_is_unmapped(space->pml, virtAddr, pageAmount))
    {
        errno = EFAULT;
        return ERR;
    }

    uint64_t callbacks[PML_MAX_CALLBACK]; // Stores the amount of pages that have each callback id within the region.
    memset(callbacks, 0, PML_MAX_CALLBACK * sizeof(uint64_t));
    pml_collect_callbacks(space->pml, virtAddr, pageAmount, callbacks);

    pml_unmap(space->pml, virtAddr, pageAmount);

    uint64_t index;
    BITMAP_FOR_EACH_SET(&index, &space->callbackBitmap)
    {
        vmm_callback_t* callback = &space->callbacks[index];
        assert(callback->pageAmount >= callbacks[index]);
        callback->pageAmount -= callbacks[index];

        if (callback->pageAmount == 0)
        {
            space->callbacks[index].func(space->callbacks[index].private);
            space_remove_callback(space, index);
        }
    }

    return 0;
}

SYSCALL_DEFINE(SYS_MUNMAP, uint64_t, void* address, uint64_t length)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!syscall_is_pointer_valid(address, length))
    {
        errno = EFAULT;
        return ERR;
    }

    return vmm_unmap(space, address, length);
}

uint64_t vmm_protect(space_t* space, void* virtAddr, uint64_t length, prot_t prot)
{
    pml_flags_t flags;
    if (vmm_prot_to_flags(prot, &flags) == ERR)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_DEFER(&space->lock);

    vmm_align_region(&virtAddr, &length);
    uint64_t pageAmount = BYTES_TO_PAGES(length);

    if (pml_is_unmapped(space->pml, virtAddr, pageAmount))
    {
        errno = EFAULT;
        return ERR;
    }

    uint64_t result = pml_set_flags(space->pml, virtAddr, pageAmount, flags);
    if (result == ERR)
    {
        errno = ENOMEM;
        return ERR;
    }

    return 0;
}

SYSCALL_DEFINE(SYS_MPROTECT, uint64_t, void* address, uint64_t length, prot_t prot)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!syscall_is_pointer_valid(address, length))
    {
        errno = EFAULT;
        return ERR;
    }

    return vmm_protect(space, address, length, prot);
}

bool vmm_mapped(space_t* space, const void* virtAddr, uint64_t length)
{
    vmm_align_region((void**)&virtAddr, &length);
    LOCK_DEFER(&space->lock);
    return pml_is_mapped(space->pml, virtAddr, BYTES_TO_PAGES(length));
}
