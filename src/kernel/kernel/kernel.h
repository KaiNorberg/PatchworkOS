#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "file_system/file_system.h"
#include "memory/memory.h"
#include "virtual_memory/virtual_memory.h"

typedef struct 
{
	Framebuffer* Screenbuffer;
	PSFFont* TTYFont;
	EFIMemoryMap* MemoryMap;
	void* RSDP;
	void* RT;
	RawDirectory* RootDirectory;
} BootInfo;

extern VirtualAddressSpace* kernelAddressSpace;

void kernel_init(BootInfo* bootInfo);