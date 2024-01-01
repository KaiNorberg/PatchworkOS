#pragma once

#include <stdint.h>

#include "gop/gop.h"
#include "file_system/file_system.h"
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
	RawDirectory* rootDirectory;
} BootInfo;

void kernel_init(BootInfo* bootInfo);

void kernel_cpu_init();