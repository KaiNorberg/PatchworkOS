#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "psf/psf.h"

typedef struct
{
	const char* Name;
	uint8_t* Data;
	uint64_t Size;
} File;

typedef struct Directory
{
	const char* Name;
	File* Files;
	uint64_t FileAmount;
	struct Directory* Directories;
	uint64_t DirectoryAmount;
} Directory;

typedef struct
{
	Framebuffer* Screenbuffer;
	PSFFont** PSFFonts;
	uint8_t FontAmount;
	void* MemoryMap;
	void* RSDP;
	void* RT;
	Directory* RootDirectory;
} BootInfo;
