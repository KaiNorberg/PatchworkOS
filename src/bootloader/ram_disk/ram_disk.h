#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct
{
	const char* name;
	uint8_t* data;
	uint64_t size;
} RamFile;

typedef struct RamDirectory
{
	const char* name;
	RamFile* files;
	uint64_t fileAmount;
	struct RamDirectory* directories;
	uint64_t directoryAmount;
} RamDirectory;

RamFile ram_disk_load_file(EFI_FILE* volume, CHAR16* path);

void ram_disk_load_directory(RamDirectory* out, EFI_FILE* volume, const char* name);
