#include "loader.h"

#include <internal/syscalls/syscalls.h>

#include <common/elf/elf.h>

#include "heap/heap.h"
#include "defs/defs.h"
#include "pmm/pmm.h"
#include "vmm/vmm.h"
#include "sched/sched.h"
#include "utils/utils.h"
#include "debug/debug.h"

void loader_init(void)
{
    //TODO: Program loader should run in kernel mode
    uint64_t pageAmount = SIZE_IN_PAGES((uint64_t)&_loaderEnd - (uint64_t)&_loaderStart);
    vmm_change_flags(&_loaderStart, pageAmount, PAGE_FLAG_USER_SUPERVISOR);
}

void* loader_allocate_stack()
{    
    Thread* thread = sched_thread();
    void* address = (void*)(VMM_LOWER_HALF_MAX - (THREAD_USER_STACK_SIZE * (thread->id + 1) + PAGE_SIZE * (thread->id)));
    if (space_allocate(sched_process()->space, address, THREAD_USER_STACK_SIZE / PAGE_SIZE) == NULL)
    {
        debug_panic("Failed to allocate user stack!");
    }
    else
    {
        return address + THREAD_USER_STACK_SIZE;
    }
}

void* loader_load()
{
    while (true)
    {
        _Test(0);
        _Sleep(1000000000);
        //spawn(executable);
        //_Process_exit(0);
    }

    /*int64_t fd = sys_open(executable, FILE_FLAG_READ);
    if (fd == -1)
    {
        sys_exit(sys_status());
    }*/

    /*ElfHeader header;
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

    return (void*)header.entry;*/
    return NULL;
}