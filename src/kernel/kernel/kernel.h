#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "ram_disk/ram_disk.h"
#include "memory/memory.h"
#include "page_directory/page_directory.h"
#include "rsdt/rsdt.h"
#include "tty/tty.h"

typedef struct 
{    
	Framebuffer* framebuffer;
	PsfFont* font;
	EfiMemoryMap* memoryMap;
	Xsdt* xsdp;
	void* rt;
	RamDirectory* rootDirectory;
} BootInfo;

void kernel_init(BootInfo* bootInfo);