#include "program_loader.h"

#include "kernel/string/string.h"
#include "kernel/file_system/file_system.h"
#include "kernel/heap/heap.h"
#include "kernel/page_allocator/page_allocator.h"
#include "kernel/multitasking/multitasking.h"

#include "kernel/tty/tty.h"

uint8_t load_program(const char* path)
{
    FILE* file = file_system_open(path, "r");

    if (file == 0)
    {
        tty_print("ERROR: Failed to open file!\n\r");
        return 0;
    }

    ElfHeader header;
    file_system_read(&header, sizeof(ElfHeader), file);

    if(header.Ident[0] != 0x7F ||
       header.Ident[1] != 'E' ||
       header.Ident[2] != 'L' ||
       header.Ident[3] != 'F')
    {
        tty_print("ERROR: Corrupt program file!\n\r");
        return 0;
    }

    uint64_t programHeaderTableSize = header.ProgramHeaderAmount * header.ProgramHeaderSize;
    ElfProgramHeader* programHeaders = kmalloc(programHeaderTableSize);
    file_system_read(programHeaders, programHeaderTableSize, file);
    
    VirtualAddressSpace* addressSpace = virtual_memory_create();
    for (uint64_t i = 0; i < page_allocator_get_total_amount(); i++)
    {
        virtual_memory_remap(addressSpace, (void*)(i * 0x1000), (void*)(i * 0x1000));
    }

    Task* task = create_task((void*)header.Entry, addressSpace);

	for (uint64_t i = 0; i < header.ProgramHeaderAmount; i++)
	{		
        switch (programHeaders[i].Type)
		{
		case PT_LOAD:
		{
            void* segment = task_allocate_memory(task, (void*)programHeaders[i].VirtualAddress, programHeaders[i].MemorySize);

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