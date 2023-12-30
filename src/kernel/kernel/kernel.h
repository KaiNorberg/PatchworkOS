#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "psf/psf.h"
#include "file_system/file_system.h"
#include "memory/memory.h"
#include "page_directory/page_directory.h"
#include "rsdt/rsdt.h"

typedef struct 
{    
	Framebuffer* framebuffer;
	PSFFont* font;
	EFIMemoryMap* memoryMap;
	Xsdt* xsdp;
	void* rt;
	RawDirectory* rootDirectory;
} BootInfo;

void kernel_init(BootInfo* bootInfo);