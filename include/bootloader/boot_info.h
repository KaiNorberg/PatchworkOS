#pragma once

#include <stdint.h>
#include <sys/list.h>

#ifndef __BOOTLOADER__

typedef struct
{
    uint32_t type;
    uint32_t pad;
    void* physicalStart;
    void* virtualStart;
    uint64_t amountOfPages;
    uint64_t attribute;
} efi_mem_desc_t;

#else

#include <efi.h>
#include <efilib.h>
typedef EFI_MEMORY_DESCRIPTOR efi_mem_desc_t;

#endif

#define EFI_RESERVED 0
#define EFI_LOADER_CODE 1
#define EFI_LOADER_DATA 2
#define EFI_BOOT_SERVICES_CODE 3
#define EFI_BOOT_SERVICES_DATA 4
#define EFI_RUNTIME_SERVICES_CODE 5
#define EFI_RUNTIME_SERVICES_DATA 6
#define EFI_CONVENTIONAL_MEMORY 7
#define EFI_UNUSABLE_MEMORY 8
#define EFI_ACPI_RECLAIM_MEMORY 9
#define EFI_ACPI_MEMORY_NVS 10
#define EFI_MEMORY_MAPPED_IO 11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE 13
#define EFI_PERSISTENT_MEMORY 14

#define EFI_IS_MEMORY_AVAIL(type) \
    ((type == EFI_CONVENTIONAL_MEMORY) || (type == EFI_PERSISTENT_MEMORY) || (type == EFI_LOADER_CODE) || \
        (type == EFI_BOOT_SERVICES_CODE) || (type == EFI_BOOT_SERVICES_DATA))

#define EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, index) \
    (efi_mem_desc_t*)((uint64_t)(memoryMap)->base + ((index) * (memoryMap)->descriptorSize))

typedef struct
{
    efi_mem_desc_t* base;
    uint64_t descriptorAmount;
    uint64_t key;
    uint64_t descriptorSize;
    uint32_t descriptorVersion;
} efi_mem_map_t;

typedef struct
{
    uint32_t* base;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} gop_buffer_t;

typedef struct ram_file
{
    list_entry_t entry;
    char name[32];
    void* data;
    uint64_t size;
} ram_file_t;

typedef struct ram_dir
{
    list_entry_t entry;
    char name[32];
    list_t children;
    list_t files;
} ram_dir_t;

typedef struct boot_info boot_info_t;

typedef struct
{
    void* physStart;
    void* virtStart;
    void (*entry)(boot_info_t*);
    uint64_t length;
} boot_kernel_t;

typedef struct boot_info
{
    efi_mem_map_t memoryMap;
    gop_buffer_t gopBuffer;
    ram_dir_t* ramRoot;
    void* rsdp;
    void* runtimeServices;
    boot_kernel_t kernel;
} boot_info_t;
