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

static inline void* loader_allocate_stack()
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

static inline void* loader_load_program()
{
    Process* process = sched_process();

    uint64_t fd = vfs_open(process->executable, O_READ);
    if (fd == ERR)
    {
        sched_process_exit(EEXEC);
    }

    ElfHeader header;
    if (vfs_read(fd, &header, sizeof(ElfHeader)) != sizeof(ElfHeader))
    {
        sched_process_exit(EEXEC);
    }
    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F')
    {
        sched_process_exit(EEXEC);
    }

    uint64_t headerTableSize = header.programHeaderAmount * header.programHeaderSize;
    ElfProgramHeader headerTable[header.programHeaderAmount];
    if (vfs_read(fd, &headerTable, headerTableSize) != headerTableSize)
    {
        sched_process_exit(EEXEC);
    }

	for (ElfProgramHeader* programHeader = headerTable;
        (uint64_t)programHeader < (uint64_t)headerTable + headerTableSize;
        programHeader = (ElfProgramHeader*)((uint64_t)programHeader + header.programHeaderSize))
	{
        switch (programHeader->type)
        {
		case PT_LOAD:
        {
            if (vfs_seek(fd, programHeader->offset, SEEK_SET) == ERR)
            {
                sched_process_exit(EEXEC);
            }

            void* address = (void*)programHeader->virtualAddress;

            if (space_allocate(process->space, address, SIZE_IN_PAGES(programHeader->memorySize) + 1) == NULL)
            {
                sched_process_exit(EEXEC);
            }

            if (vfs_read(fd, address, programHeader->fileSize) == ERR)
            {
                sched_process_exit(EEXEC);
            }
		}
		break;
		}
	}

    if (vfs_close(fd) == ERR)
    {
        sched_process_exit(EEXEC);
    }

    return (void*)header.entry;
}

void loader_entry()
{
    void* rsp = loader_allocate_stack();
    void* rip = loader_load_program();

    loader_jump_to_user_space(rsp, rip);
}