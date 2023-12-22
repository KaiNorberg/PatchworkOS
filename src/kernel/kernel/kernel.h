#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "file_system/file_system.h"
#include "memory/memory.h"
#include "page_directory/page_directory.h"
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