#include "program_loader.h"

#include "ram_disk/ram_disk.h"
#include "heap/heap.h"
#include "page_allocator/page_allocator.h"
#include "gdt/gdt.h"
#include "tty/tty.h"
#include "debug/debug.h"
#include "worker_pool/worker_pool.h"
#include "utils/utils.h"

#include "vfs/vfs.h"

#include <libc/string.h>

#include "io/io.h"
#include "hpet/hpet.h"

Status load_program(Process* process, File* file)
{
    //This still sucks, il fix it one day.

    ElfHeader header;
    Status status = vfs_read(file, &header, sizeof(ElfHeader));
    if (status != STATUS_SUCCESS)
    {
        return status;
    }

    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F')
    {
        return STATUS_CORRUPT;
    }

    uint64_t programHeaderTableSize = header.programHeaderAmount * header.programHeaderSize;
    ElfProgramHeader* programHeaders = kmalloc(programHeaderTableSize);
    if (programHeaders == 0)
    {
        return STATUS_FAILURE;
    }
    status = vfs_read(file, programHeaders, programHeaderTableSize);
    if (status != STATUS_SUCCESS)
    {    
        kfree(programHeaders);
        return status;
    }

	uint64_t start = -1;
	uint64_t end = 0;
	for (ElfProgramHeader* programHeader = programHeaders; 
        (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; 
        programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
		switch (programHeader->type)
		{
		case PT_LOAD:
		{
            start = MIN(start, programHeader->virtualAddress);
            end = MAX(end, programHeader->virtualAddress + programHeader->memorySize);
		}
		break;
		}
	}

    uint64_t pageAmount = GET_SIZE_IN_PAGES(end - start);
    void* buffer = process_allocate_pages(process, (void*)start, pageAmount);
    memset(buffer, 0, pageAmount * 0x1000);

	for (ElfProgramHeader* programHeader = programHeaders; 
        (uint64_t)programHeader < (uint64_t)programHeaders + programHeaderTableSize; 
        programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
        switch (programHeader->type)
        {
		case PT_LOAD:
        {
            void* physicalAddress = (void*)((uint64_t)buffer + (programHeader->virtualAddress - start));

            status = vfs_seek(file, programHeader->offset, FILE_SEEK_SET);    
            if (status != STATUS_SUCCESS)
            {    
                kfree(programHeaders);
                return status;
            }
            
            status = vfs_read(file, physicalAddress, programHeader->fileSize);
            if (status != STATUS_SUCCESS)
            {    
                kfree(programHeaders);
                return status;
            }
		}
		break;
		}
	}

    process->interruptFrame->instructionPointer = header.entry;

    kfree(programHeaders);
    return STATUS_SUCCESS;
}