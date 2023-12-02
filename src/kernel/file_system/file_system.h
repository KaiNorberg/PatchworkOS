#pragma once

#include <stdint.h>

// Disclaimer: This "file system" is not really a file system. 
// The way it works is that the boot loader caches all files in the boot media and then sends it to the kernel.
// This is terrible for many reasons but it allows me limit the scope of this project and to move on to things i find more interesting.
// In the future i might implement a real file system. 

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct
{
	const char* Name;
	uint8_t* Data;
	uint64_t Size;
} RawFile;

typedef struct RawDirectory
{
	const char* Name;
	RawFile* Files;
	uint64_t FileAmount;
	struct RawDirectory* Directories;
	uint64_t DirectoryAmount;
} RawDirectory;

typedef struct
{
	RawFile* FileHandle;
	uint64_t SeekOffset;
} FILE;

void file_system_init(RawDirectory* rootDirectory);

uint8_t file_system_compare_names(const char* nameStart, const char* nameEnd, const char* otherName);

RawFile* file_system_get(const char* path);

FILE* file_system_open(const char* filename, const char* mode);

uint32_t file_system_seek(FILE *stream, int64_t offset, uint32_t origin);

uint64_t file_system_tell(FILE *stream);

uint32_t file_system_get_c(FILE* stream);

uint64_t file_system_read(void* buffer, uint64_t size, FILE* stream);

uint32_t file_system_close(FILE* stream);