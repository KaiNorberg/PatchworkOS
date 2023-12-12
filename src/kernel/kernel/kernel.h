#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "file_system/file_system.h"
#include "memory/memory.h"
#include "virtual_memory/virtual_memory.h"
#include "rsdp/rsdp.h"

typedef struct 
{
	Framebuffer* Screenbuffer;
	PSFFont* TTYFont;
	EFIMemoryMap* MemoryMap;
	XSDP* Xsdp;
	void* RT;
	RawDirectory* RootDirectory;
} BootInfo;

void kernel_init(BootInfo* bootInfo);