#include "program_loader.h"

#include "string/string.h"
#include "file_system/file_system.h"
#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "scheduler/scheduler.h"

#include "tty/tty.h"
#include "debug/debug.h"

uint8_t load_program(const char* path)
{
    FILE* file = file_system_open(path, "r");

    if (file == 0)
    {
        debug_panic(path);
        return 0;
    }

    ElfHeader header;
    file_system_read(&header, sizeof(ElfHeader), file);

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
    file_system_read(programHeaders, programHeaderTableSize, file);

    Process* process = process_new((void*)header.entry);

	for (uint64_t i = 0; i < header.programHeaderAmount; i++)
	{		
        switch (programHeaders[i].type)
		{
		case PT_LOAD:
		{
            void* segment = process_allocate_pages(process, (void*)programHeaders[i].virtualAddress, GET_SIZE_IN_PAGES(programHeaders[i].memorySize));

            file_system_seek(file, programHeaders[i].offset, SEEK_SET);
            file_system_read(segment, programHeaders[i].memorySize, file);
		}
		break;
		}
	}

    scheduler_append(process);

    kfree(programHeaders);
    file_system_close(file);

    return 1;
}