#pragma once

#include <stdint.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

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

typedef struct
{
	RamFile* fileHandle;
	uint64_t seekOffset;
} FILE;

void ram_disk_init(RamDirectory* rootDirectory);

uint8_t ram_disk_compare_names(const char* nameStart, const char* nameEnd, const char* otherName);

RamFile* ram_disk_get(const char* path);

FILE* ram_disk_open(const char* filename);

uint32_t ram_disk_seek(FILE *stream, int64_t offset, uint32_t origin);

uint64_t ram_disk_tell(FILE *stream);

uint32_t ram_disk_get_c(FILE* stream);

uint64_t ram_disk_read(void* buffer, uint64_t size, FILE* stream);

uint32_t ram_disk_close(FILE* stream);