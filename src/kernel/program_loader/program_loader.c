#include "program_loader.h"

#include "string/string.h"
#include "ram_disk/ram_disk.h"
#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "scheduler/scheduler.h"

#include "tty/tty.h"
#include "debug/debug.h"

uint8_t load_program(const char* path)
{
    FILE* file = ram_disk_open(path);

    if (file == 0)
    {
        debug_panic(path);
        return 0;
    }

    ElfHeader header;
    ram_disk_read(&header, sizeof(ElfHeader), file);

    if(header.ident[0] != 0x7F ||
       header.ident[1] != 'E' ||
       header.ident[2] != 'L' ||
       header.ident[3] != 'F')
    {
        debug_panic("Corrupt program file!\n\r");
        return 0;
    }

    uint64_t programHeaderTableSize = header.programHeaderAmount * header.programHeaderSize;
    ElfProgramHeader* programHeaders = kmalloc(programHeaderTableSize);
    if (programHeaders == 0)
    {
        debug_panic("Failed to allocate memory for program headers!");
    }
    ram_disk_read(programHeaders, programHeaderTableSize, file);

    Process* process = process_new((void*)header.entry);

	for (ElfProgramHeader* programHeader = programHeaders; (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{		
        switch (programHeader->type)
		{
		case PT_LOAD:
		{
            void* segment = process_allocate_pages(process, (void*)programHeader->virtualAddress, GET_SIZE_IN_PAGES(programHeader->memorySize));

            ram_disk_seek(file, programHeader->offset, SEEK_SET);
            ram_disk_read(segment, programHeader->fileSize, file);
		}
		break;
		}
	}

    scheduler_append(process);

    kfree(programHeaders);
    ram_disk_close(file);

    return 1;
}