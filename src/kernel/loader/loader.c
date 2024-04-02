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
    void* address = (void*)(VMM_LOWER_HALF_MAX - (CONFIG_USER_STACK * (thread->id + 1) + PAGE_SIZE * (thread->id)));
    if (vmm_allocate(address, CONFIG_USER_STACK / PAGE_SIZE) == NULL)
    {
        debug_panic("Failed to allocate user stack!");
    }
    else
    {
        return address + CONFIG_USER_STACK;
    }
}

static inline void* loader_load_program()
{
    uint64_t fd = vfs_open(sched_process()->executable);
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

            if (vmm_allocate(address, SIZE_IN_PAGES(programHeader->memorySize) + 1) == NULL)
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