#include <kernel/init/boot_info.h>

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

static void boot_dir_to_higher_half(boot_dir_t* dir)
{
    // Pop everything from the lists and then readd them with higher half addresses.
    list_t children = LIST_CREATE(children);
    list_t files = LIST_CREATE(files);

    list_t* dirChildren = (list_t*)PML_ENSURE_LOWER_HALF(&dir->children);
    list_t* dirFiles = (list_t*)PML_ENSURE_LOWER_HALF(&dir->files);

    while (!list_is_empty(dirChildren))
    {
        boot_dir_t* child = CONTAINER_OF(list_pop_first(dirChildren), boot_dir_t, entry);

        child = (boot_dir_t*)PML_ENSURE_HIGHER_HALF(child);
        child->entry = LIST_ENTRY_CREATE(child->entry);

        list_push_back(&children, &child->entry);
    }

    while (!list_is_empty(dirFiles))
    {
        boot_file_t* file = CONTAINER_OF(list_pop_first(dirFiles), boot_file_t, entry);

        file = (boot_file_t*)PML_ENSURE_HIGHER_HALF(file);
        file->entry = LIST_ENTRY_CREATE(file->entry);
        file->data = (void*)PML_ENSURE_HIGHER_HALF(file->data);

        list_push_back(&files, &file->entry);
    }

    dir->children = LIST_CREATE(dir->children);
    dir->files = LIST_CREATE(dir->files);

    while (!list_is_empty(&children))
    {
        boot_dir_t* child = CONTAINER_OF_SAFE(list_pop_first(&children), boot_dir_t, entry);
        list_push_back(&dir->children, &child->entry);
        boot_dir_to_higher_half(child);
    }

    while (!list_is_empty(&files))
    {
        boot_file_t* file = CONTAINER_OF_SAFE(list_pop_first(&files), boot_file_t, entry);
        list_push_back(&dir->files, &file->entry);
    }
}

void boot_info_to_higher_half(void)
{
    bootInfo->gop.physAddr = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->gop.physAddr);
    bootInfo->gop.virtAddr = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->gop.virtAddr);

    bootInfo->rsdp = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->rsdp);

    bootInfo->runtimeServices = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->runtimeServices);

    bootInfo->disk.root = (boot_dir_t*)PML_ENSURE_HIGHER_HALF(bootInfo->disk.root);
    boot_dir_to_higher_half(bootInfo->disk.root);

    bootInfo->kernel.physAddr = (void*)PML_ENSURE_HIGHER_HALF(bootInfo->kernel.physAddr);

    bootInfo->memory.map.descriptors = (EFI_MEMORY_DESCRIPTOR*)PML_ENSURE_HIGHER_HALF(bootInfo->memory.map.descriptors);

    bootInfo = (boot_info_t*)PML_ENSURE_HIGHER_HALF(bootInfo);
}

void boot_info_free(void)
{
    // The memory map will be stored in the data we are freeing so we copy it first.
    boot_memory_map_t volatile mapCopy = bootInfo->memory.map;
    uint64_t descriptorsSize = bootInfo->memory.map.descSize * bootInfo->memory.map.length;
    EFI_MEMORY_DESCRIPTOR* volatile descriptorsCopy = malloc(descriptorsSize);
    if (descriptorsCopy == NULL)
    {
        panic(NULL, "Failed to allocate memory for boot memory map copy");
    }
    memcpy_s(descriptorsCopy, descriptorsSize, bootInfo->memory.map.descriptors, descriptorsSize);
    mapCopy.descriptors = (EFI_MEMORY_DESCRIPTOR* const)descriptorsCopy;

    for (uint64_t i = 0; i < mapCopy.length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(&mapCopy, i);

        if (desc->Type == EfiLoaderData)
        {
            LOG_INFO("free boot memory [0x%016lx-0x%016lx]\n", desc->VirtualStart,
                (uintptr_t)desc->VirtualStart + (desc->NumberOfPages * PAGE_SIZE));
#ifndef NDEBUG
            // Clear the memory to deliberately cause corruption if the memory is actually being used.
            memset((void*)desc->VirtualStart, 0xCC, desc->NumberOfPages * PAGE_SIZE);
#endif
            pmm_free_region((void*)desc->VirtualStart, desc->NumberOfPages);
        }
    }

    free(descriptorsCopy);

    bootInfo = NULL;
}