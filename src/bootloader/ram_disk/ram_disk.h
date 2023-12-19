#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct
{
	const char* name;
	uint8_t* data;
	uint64_t size;
} RamDiskFile;

typedef struct RamDiskDirectory
{
	const char* name;
	RamDiskFile* files;
	uint64_t fileAmount;
	struct RamDiskDirectory* directories;
	uint64_t directoryAmount;
} RamDiskDirectory;

RamDiskFile ram_disk_load_file(EFI_FILE* volume, CHAR16* path);

RamDiskDirectory ram_disk_load_directory(EFI_FILE* volume, const char* name);