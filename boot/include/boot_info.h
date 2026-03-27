#pragma once

#include <kernel/mem/paging_types.h>

#include <gnu-efi/efi.h>

#include <_libstd/MAX_NAME.h>
#include <libstd/defs.h>
#include <libstd/elf.h>
#include <libstd/list.h>
#include <stdint.h>

/**
 * @brief Boot information.
 * @defgroup boot_info Boot Information
 * @ingroup boot
 *
 * The boot information structure is used to pass information from the bootloader to the kernel, such as memory map, or
 * `rsdp`.
 *
 * @{
 */

static UNUSED_FUNC bool boot_is_mem_ram(EFI_MEMORY_TYPE type)
{
    switch (type)
    {
    case EfiConventionalMemory:
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:
    case EfiRuntimeServicesCode:
    case EfiRuntimeServicesData:
    case EfiACPIReclaimMemory:
    case EfiACPIMemoryNVS:
        return true;
    default:
        return false;
    }
}
typedef struct
{
    phys_addr_t physAddr;
    uint32_t* virtAddr;
    size_t size;
    size_t width;
    size_t height;
    size_t stride;
} boot_gop_t;

#define BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, index) \
    (EFI_MEMORY_DESCRIPTOR*)((uint64_t)(map)->descriptors + ((index) * (map)->descSize))

typedef struct
{
    EFI_MEMORY_DESCRIPTOR* descriptors;
    size_t length;
    UINTN descSize;
    UINT32 descVersion;
    UINTN key;
} boot_memory_map_t;

typedef struct boot_initrd
{
    void* buffer;
    size_t size;
} boot_initrd_t;

typedef struct boot_info boot_info_t;

typedef struct
{
    Elf64_File elf;
    phys_addr_t physAddr;
} boot_kernel_t;

typedef struct
{
    void* buffer;
    size_t size;
    uintptr_t loadAddr;
    uintptr_t entry;
} boot_init_t;

typedef struct
{
    boot_memory_map_t map;
    page_table_t table;
} boot_memory_t;

typedef struct boot_info
{
    boot_gop_t gop;
    void* rsdp;
    void* runtimeServices;
    boot_initrd_t initrd;
    boot_kernel_t kernel;
    boot_init_t init;
    boot_memory_t memory;
} boot_info_t;

/** @} */
