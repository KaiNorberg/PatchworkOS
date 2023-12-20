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
	Framebuffer* framebuffer;
	PSFFont* font;
	EFIMemoryMap* memoryMap;
	XSDP* xsdp;
	void* rt;
	RawDirectory* rootDirectory;
} BootInfo;

void kernel_init(BootInfo* bootInfo);