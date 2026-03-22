#include <kernel/start/boot_info.h>

#include <kernel/fs/sysfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>

#include <boot/boot_info.h>

#include <stdlib.h>
#include <string.h>

// Set in _start()
boot_info_t* bootInfo = NULL;

boot_info_t* boot_info_get(void)
{
    return bootInfo;
}

void boot_info_to_higher_half(void)
{
    bootInfo->gop.physAddr = PML_ENSURE_HIGHER_HALF(bootInfo->gop.physAddr);
    bootInfo->gop.virtAddr = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->gop.virtAddr);

    bootInfo->rsdp = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->rsdp);

    bootInfo->runtimeServices = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->runtimeServices);

    bootInfo->initrd.buffer = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->initrd.buffer);

    bootInfo->kernel.physAddr = PML_ENSURE_HIGHER_HALF(bootInfo->kernel.physAddr);

    bootInfo->memory.map.descriptors = (EFI_MEMORY_DESCRIPTOR*)PML_ENSURE_HIGHER_HALF(bootInfo->memory.map.descriptors);

    bootInfo = (boot_info_t*)PML_ENSURE_HIGHER_HALF(bootInfo);
}

static dentry_t* initrdDentry = NULL;
static boot_initrd_t initrdData;

static status_t boot_info_read_initrd(irp_t* irp)
{
    return irp_read_helper(irp, initrdData.buffer, initrdData.size);
}

static status_t boot_info_seek_initrd(irp_t* irp)
{
    return irp_seek_helper(irp, initrdData.size);
}

static vnode_class_t initrdClass = {
    .name = "initrd",
    .type = FILE_TYPE_SYSTEM,
    .handlers =
        {
            VNODE_HANDLERS(),
            [IRP_MJ_READ] = boot_info_read_initrd,
            [IRP_MJ_SEEK] = boot_info_seek_initrd,
        },
};

void boot_info_expose_initrd(void)
{
    if (bootInfo->initrd.buffer == NULL || bootInfo->initrd.size == 0)
    {
        panic(NULL, "Initrd buffer is NULL or size is 0");
    }

    initrdData.buffer = malloc(bootInfo->initrd.size);
    if (initrdData.buffer == NULL)
    {
        panic(NULL, "Failed to allocate memory for initrd");
    }
    memcpy_s(initrdData.buffer, bootInfo->initrd.size, bootInfo->initrd.buffer, bootInfo->initrd.size);
    initrdData.size = bootInfo->initrd.size;

    initrdDentry = sysfs_dentry_new(NULL, "initrd", &initrdClass, NULL);
    if (initrdDentry == NULL)
    {
        panic(NULL, "Failed to create initrd dentry");
    }
}

void boot_info_free(void)
{
    // The memory map will be stored in the data we are freeing so we copy it first.
    boot_memory_map_t volatile mapCopy = bootInfo->memory.map;
    size_t descriptorsSize = bootInfo->memory.map.descSize * bootInfo->memory.map.length;
    EFI_MEMORY_DESCRIPTOR* volatile descriptorsCopy = malloc(descriptorsSize);
    if (descriptorsCopy == NULL)
    {
        panic(NULL, "Failed to allocate memory for boot memory map copy");
    }
    memcpy_s(descriptorsCopy, descriptorsSize, bootInfo->memory.map.descriptors, descriptorsSize);
    mapCopy.descriptors = (EFI_MEMORY_DESCRIPTOR* const)descriptorsCopy;

    for (size_t i = 0; i < mapCopy.length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(&mapCopy, i);

        if (desc->Type == EfiLoaderData)
        {
            LOG_INFO("free boot memory [%p-%p]\n", desc->VirtualStart,
                (uintptr_t)desc->VirtualStart + (desc->NumberOfPages * PAGE_SIZE));
#ifndef NDEBUG
            // Clear the memory to deliberately cause corruption if the memory is actually being used.
            memset((void*)desc->VirtualStart, 0xCC, desc->NumberOfPages * PAGE_SIZE);
#endif
            pmm_free_region(VIRT_TO_PFN(desc->VirtualStart), desc->NumberOfPages);
        }
    }

    free(descriptorsCopy);

    bootInfo = NULL;
}
