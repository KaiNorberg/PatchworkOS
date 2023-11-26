#pragma once

#include <stdint.h>

typedef struct
{
	const char* Name;
	uint8_t* Data;
	uint64_t Size;
} FileContent;

typedef struct RawDirectory
{
	const char* Name;
	FileContent* Files;
	uint64_t FileAmount;
	struct RawDirectory* Directories;
	uint64_t DirectoryAmount;
} RawDirectory;

void file_system_init(RawDirectory* rootDirectory);

uint8_t file_system_compare_names(const char* nameStart, const char* nameEnd, const char* otherName);

FileContent* file_system_get(const char* path);