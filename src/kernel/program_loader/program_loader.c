#include "program_loader.h"

#include <stdint.h>

#include <libc/string.h>

#include <lib-asym.h>

#include <common/elf/elf.h>

#include "heap/heap.h"
#include "pmm/pmm.h"
#include "vmm/vmm.h"
#include "utils/utils.h"
#include "vfs/vfs.h"

void program_loader_init()
{
    uint64_t pageAmount = SIZE_IN_PAGES((uint64_t)&_programLoaderEnd - (uint64_t)&_programLoaderStart);
    vmm_change_flags(&_programLoaderStart, pageAmount, PAGE_FLAG_USER_SUPERVISOR);
}

void* program_loader_load(const char* executable)
{
    while (1)
    {
        sys_test(executable);
    }

    int64_t fd = open(executable, FILE_FLAG_READ);
    if (fd == -1)
    {
        exit(status());
    }

    ElfHeader header;
    if (read(fd, &header, sizeof(ElfHeader)) == -1)
    {
        exit(status());
    }
    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F')
    {
        exit(STATUS_CORRUPT);
    }

    uint64_t programHeaderTableSize = header.programHeaderAmount * header.programHeaderSize;    
    ElfProgramHeader programHeaders[header.programHeaderAmount];
    if (read(fd, &programHeaders, programHeaderTableSize) == -1)
    {
        exit(status());
    }

	for (ElfProgramHeader* programHeader = programHeaders; 
        (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; 
        programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
        switch (programHeader->type)
        {
		case PT_LOAD:
        {
            if (seek(fd, programHeader->offset, FILE_SEEK_SET) == -1)
            {
                exit(status());
            }

            if (map((void*)programHeader->virtualAddress, (void*)(programHeader->virtualAddress + programHeader->memorySize)) == -1)
            {
                exit(status());
            }

            if (read(fd, (void*)programHeader->virtualAddress, programHeader->fileSize) == -1)
            {
                exit(status());
            }
		}
		break;
		}
	}

    if (close(fd) == -1)
    {
        exit(status());
    }

    return (void*)header.entry;
}