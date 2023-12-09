#include "program_loader.h"

#include "string/string.h"
#include "file_system/file_system.h"
#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "multitasking/multitasking.h"

#include "tty/tty.h"
#include "debug/debug.h"

uint8_t load_program(const char* path)
{
    FILE* file = file_system_open(path, "r");

    if (file == 0)
    {
        debug_error("Failed to open file ("); tty_print(path); tty_print(")!\n\r");
        return 0;
    }

    ElfHeader header;
    file_system_read(&header, sizeof(ElfHeader), file);

    if(header.Ident[0] != 0x7F ||
       header.Ident[1] != 'E' ||
       header.Ident[2] != 'L' ||
       header.Ident[3] != 'F')
    {
        debug_error("Corrupt program file!\n\r");
        return 0;
    }

    uint64_t programHeaderTableSize = header.ProgramHeaderAmount * header.ProgramHeaderSize;
    ElfProgramHeader* programHeaders = kmalloc(programHeaderTableSize);
    file_system_read(programHeaders, programHeaderTableSize, file);
    
    Task* task = multitasking_new((void*)header.Entry);

	for (uint64_t i = 0; i < header.ProgramHeaderAmount; i++)
	{		
        switch (programHeaders[i].Type)
		{
		case PT_LOAD:
		{
            void* segment = task_allocate_pages(task, (void*)programHeaders[i].VirtualAddress, programHeaders[i].MemorySize / 0x1000 + 1);

            file_system_seek(file, programHeaders[i].Offset, SEEK_SET);
            file_system_read(segment, programHeaders[i].MemorySize, file);
		}
		break;
		}
	}

    kfree(programHeaders);
    file_system_close(file);

    return 1;
}