#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct
{
	EFI_MEMORY_DESCRIPTOR* base;
	uint64_t descriptorAmount;
	uint64_t descriptorSize;
	uint64_t key;
} EfiMemoryMap;

void* memory_allocate_pages(uint64_t pageAmount);

EfiMemoryMap memory_get_map();
