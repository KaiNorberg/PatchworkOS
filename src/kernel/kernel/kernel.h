#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "memory/memory.h"
#include "page_directory/page_directory.h"
#include "rsdt/rsdt.h"
#include "tty/tty.h"
#include "ram_disk/ram_disk.h"

typedef struct 
{    
	Framebuffer framebuffer;
	PsfFont font;
	EfiMemoryMap memoryMap;
	RamDirectory* ramRoot;
	Xsdt* xsdp;
	void* rt;
} BootInfo;

void kernel_init(BootInfo* bootInfo);