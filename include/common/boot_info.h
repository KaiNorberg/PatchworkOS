#pragma once

#include <stdint.h>
#include <sys/gfx.h>

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

#else

#include <efi.h>
#include <efilib.h>
typedef EFI_MEMORY_DESCRIPTOR efi_mem_desc_t;

#endif

#define EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, index) \
    (efi_mem_desc_t*)((uint64_t)(memoryMap)->base + ((index) * (memoryMap)->descriptorSize))

#define EFI_MEM_KERNEL 0x80000000
#define EFI_MEM_BOOT_PML 0x80000001
#define EFI_MEM_BOOT_INFO 0x80000002
#define EFI_MEM_RAM_DISK 0x80000003
#define EFI_MEM_MEMORY_MAP 0x80000004

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

typedef struct ram_file_t
{
    char name[32];
    void* data;
    uint64_t size;
    struct ram_file_t* next;
    struct ram_file_t* prev;
} ram_file_t;

typedef struct ram_dir_t
{
    char name[32];
    ram_file_t* firstFile;
    ram_file_t* lastFile;
    struct ram_dir_t* firstChild;
    struct ram_dir_t* lastChild;
    struct ram_dir_t* next;
    struct ram_dir_t* prev;
} ram_dir_t;

typedef struct
{
    efi_mem_map_t memoryMap;
    gop_buffer_t gopBuffer;
    ram_dir_t* ramRoot;
    void* rsdp;
    void* runtimeServices;
} boot_info_t;
