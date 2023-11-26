#pragma once

#include <stdint.h>

#include "kernel/gop/gop.h"
#include "kernel/psf/psf.h"
#include "kernel/file_system/file_system.h"

typedef struct
{
	Framebuffer* Screenbuffer;
	PSFFont** PSFFonts;
	uint8_t FontAmount;
	void* MemoryMap;
	void* RSDP;
	void* RT;
	RawDirectory* RootDirectory;
} BootInfo;
