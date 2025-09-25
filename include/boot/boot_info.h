#pragma once

#include <common/paging.h>

#include <gnu-efi/inc/efi.h>
#include <gnu-efi/inc/efilib.h>

#include <stdint.h>
#include <sys/elf.h>
#include <sys/io.h>
#include <sys/list.h>

static bool boot_is_mem_ram(EFI_MEMORY_TYPE type)
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
    uint32_t* physAddr;
    uint32_t* virtAddr;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} boot_gop_t;

#define BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, index) \
    (EFI_MEMORY_DESCRIPTOR*)((uint64_t)(map)->descriptors + ((index) * (map)->descSize))

typedef struct
{
    EFI_MEMORY_DESCRIPTOR* descriptors;
    uint64_t length;
    UINTN descSize;
    UINT32 descVersion;
    UINTN key;
} boot_memory_map_t;

typedef struct boot_dir boot_dir_t;

typedef struct boot_file
{
    list_entry_t entry;
    char name[MAX_NAME];
    void* data;
    uint64_t size;
} boot_file_t;

typedef struct boot_dir
{
    list_entry_t entry;
    char name[MAX_NAME];
    list_t children;
    list_t files;
} boot_dir_t;

typedef struct boot_disk
{
    boot_dir_t* root;
} boot_disk_t;

typedef struct boot_info boot_info_t;

typedef struct
{
    elf_hdr_t header;
    elf_phdr_t* phdrs;
    elf_shdr_t* shdrs;
    uint32_t shdrCount;
    elf_sym_t* symbols;
    uint32_t symbolCount;
    char* stringTable;
    uint64_t stringTableSize;
    uintptr_t physStart;
    uintptr_t virtStart;
    void (*entry)(boot_info_t*);
    uint64_t size;
} boot_kernel_t;

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
    boot_disk_t disk;
    boot_kernel_t kernel;
    boot_memory_t memory;
} boot_info_t;
