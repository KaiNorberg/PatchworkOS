#pragma once

#include <stdint.h>

#include "kernel/gop/gop.h"
#include "kernel/psf/psf.h"
#include "kernel/file_system/file_system.h"
#include "kernel/memory/memory.h"

typedef struct
{
	Framebuffer* Screenbuffer;
	BootFont* TTYFont;
	EFIMemoryMap* MemoryMap;
	void* RSDP;
	void* RT;
	RawDirectory* RootDirectory;
} BootInfo;

void kernel_init(BootInfo* bootInfo);