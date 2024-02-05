#include "program_loader.h"

#include "ram_disk/ram_disk.h"
#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "gdt/gdt.h"
#include "tty/tty.h"
#include "debug/debug.h"
#include "worker_pool/worker_pool.h"

#include "vfs/vfs.h"

#include <libc/string.h>

uint8_t load_program(Task* task, const char* path)
{
    //This sucks, dont worry about it
    File* file;
    Status status = vfs_open(&file, path, VFS_FLAG_READ);
    if (status != STATUS_SUCCESS)
    {
        return 0;
    }

    if (file == 0)
    {
        debug_panic(path);
        return 0;
    }

    ElfHeader header;
    vfs_read(file, &header, sizeof(ElfHeader));

    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F')
    {
        debug_panic("Corrupt program file!\n\r");
        return 0;
    }

    uint64_t programHeaderTableSize = header.programHeaderAmount * header.programHeaderSize;
    ElfProgramHeader* programHeaders = kmalloc(programHeaderTableSize);
    if (programHeaders == 0)
    {
        debug_panic("Failed to allocate memory for program headers!");
        return 0;
    }
    vfs_read(file, programHeaders, programHeaderTableSize);

	for (ElfProgramHeader* programHeader = programHeaders; 
        (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; 
        programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{		
        switch (programHeader->type)
        {
		case PT_LOAD:
        {
            uint64_t pageAmount = GET_SIZE_IN_PAGES(programHeader->memorySize);
            for (uint64_t i = 0; i < pageAmount; i++)
            {
                void* segment = process_allocate_page(task->process, (void*)programHeader->virtualAddress + i * 0x1000);
                memset(segment, 0, 0x1000);

                vfs_seek(file, programHeader->offset + i * 0x1000);
                if (i == pageAmount - 1)
                {
                    vfs_read(file, segment, programHeader->fileSize % 0x1000);
                }
                else
                {
                    vfs_read(file, segment, 0x1000);
                }
            }
		}
		break;
		}
	}

    task->interruptFrame->instructionPointer = header.entry;

    kfree(programHeaders);
    vfs_close(file);

    return 1;
}