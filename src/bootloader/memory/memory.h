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

EfiMemoryMap memory_get_map();
