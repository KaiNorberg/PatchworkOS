#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct
{
	EFI_MEMORY_DESCRIPTOR* base;
	uint64_t descriptorAmount;
	uint64_t key;
	uint64_t descriptorSize;
	uint32_t descriptorVersion;
} EfiMemoryMap;

void* memory_allocate_pages(uint64_t pageAmount, uint64_t memoryType);

void* memory_allocate_pool(uint64_t size, uint64_t memoryType);

void memory_get_map(EfiMemoryMap* memoryMap);
