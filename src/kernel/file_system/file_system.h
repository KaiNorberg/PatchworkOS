#pragma once

#include <stdint.h>

// Disclaimer: This "file system" is not really a file system. 
// The way it works is that the boot loader caches all files in the boot media and then sends it to the kernel.
// This is terrible for many reasons but it allows me limit the scope of this project and to move on to things i find more interesting.
// In the future i might implement a real file system. 

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