#pragma once

#include <stdint.h>

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

void ram_disk_init(RamDirectory* root);