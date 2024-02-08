#pragma once

#include <efi.h>
#include <efilib.h>

typedef struct RamFile
{
	char name[32];
	void* data;
	uint64_t size;
	uint64_t pageAmount;
	struct RamFile* next;
	struct RamFile* prev;
} RamFile;

typedef struct RamDirectory
{
	char name[32];
	RamFile* firstFile;
	RamFile* lastFile;
	struct RamDirectory* firstChild;
	struct RamDirectory* lastChild;
	struct RamDirectory* next;
	struct RamDirectory* prev;
} RamDirectory;

RamFile* ram_disk_load_file(EFI_FILE* volume, CHAR16* path);

RamDirectory* ram_disk_load_directory(EFI_FILE* volume, const char* name);
