#pragma once

#include <stdint.h>

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

void ram_disk_init(RamDirectory* root);