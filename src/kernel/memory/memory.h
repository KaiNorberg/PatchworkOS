#pragma once

#include <stdint.h>

#define EFI_GET_DESCRIPTOR(memoryMap, index) (EFIMemoryDescriptor*)((uint64_t)memoryMap->base + (index * memoryMap->descriptorSize))

typedef struct
{
	uint32_t type;
	void* physicalStart;
	void* virtualStart;
	uint64_t amountOfPages;
	uint64_t attribute;
} EFIMemoryDescriptor;

typedef struct
{
	EFIMemoryDescriptor* base;
	uint64_t descriptorAmount;
	uint64_t key;
	uint64_t descriptorSize;
	uint32_t descriptorVersion;
} EfiMemoryMap;

typedef enum
{
	EFI_RESERVED_MEMORY_TYPE,
	EFI_LOADER_CODE,
	EFI_LOADER_DATA,
	EFI_BOOT_SERVICES_CODE,
	EFI_BOOT_SERVICES_DATA,
	EFI_RUNTIME_SERVICES_CODE,
	EFI_RUNTIME_SERVICES_DATA,
	EFI_CONVENTIONAL_MEMORY,
	EFI_UNUSABLE_MEMORY,
	EFI_ACPI_RECLAIM_MEMORY,
	EFI_ACPI_MEMORY_NVS,
	EFI_MEMORY_MAPPED_IO,
	EFI_MEMORY_MAPPED_IO_PORT_SPACE,
	EFI_PAL_CODE,
	EFI_PERSISTENT_MEMORY,
	EFI_MAX_MEMORY_TYPE
} EFIMemoryType;

uint8_t is_memory_type_reserved(uint64_t memoryType);
