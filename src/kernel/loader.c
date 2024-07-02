#include "loader.h"

#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>

#include "errno.h"
#include "sched.h"
#include "vmm.h"
#include "log.h"

static void* loader_allocate_stack(void)
{
    thread_t* thread = sched_thread();

    void* address = (void*)(VMM_LOWER_HALF_MAX - (CONFIG_USER_STACK * (thread->id + 1) + PAGE_SIZE * (thread->id)));
    if (vmm_alloc(address, CONFIG_USER_STACK, PROT_READ | PROT_WRITE) == NULL)
    {
        sched_process_exit(EEXEC);
    }

    return address + CONFIG_USER_STACK;
}

static void* loader_load_program(void)
{
    const char* executable = sched_process()->executable;
    file_t* file = vfs_open(executable);
    if (file == NULL)
    {
        sched_process_exit(EEXEC);
    }
    FILE_GUARD(file);

    char parentDir[MAX_PATH];
    vfs_parent_dir(parentDir, executable);
    if (vfs_chdir(parentDir) == ERR)
    {
        sched_process_exit(EEXEC);
    }

    elf_hdr_t header;
    if (FILE_CALL(file, read, &header, sizeof(elf_hdr_t)) != sizeof(elf_hdr_t))
    {
        sched_process_exit(EEXEC);
    }
    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F')
    {
        sched_process_exit(EEXEC);
    }

    for (uint64_t i = 0; i < header.programHeaderAmount; i++)
    {
        uint64_t offset = sizeof(elf_hdr_t) + header.programHeaderSize * i;
        if (FILE_CALL(file, seek, offset, SEEK_SET) != offset)
        {
            sched_process_exit(EEXEC);
        }

        elf_phdr_t programHeader;
        if (FILE_CALL(file, read, &programHeader, sizeof(elf_phdr_t)) != sizeof(elf_phdr_t))
        {
            sched_process_exit(EEXEC);
        }

        switch (programHeader.type)
        {
        case PT_LOAD:
        {
            uint64_t size = MAX(programHeader.memorySize, programHeader.fileSize);

            if (vmm_alloc((void*)programHeader.virtAddr, size, PROT_READ | PROT_WRITE) == NULL)
            {
                sched_process_exit(EEXEC);
            }

            if (FILE_CALL(file, seek, programHeader.offset, SEEK_SET) != programHeader.offset)
            {
                sched_process_exit(EEXEC);
            }

            memset((void*)programHeader.virtAddr, 0, size);
            if (FILE_CALL(file, read, (void*)programHeader.virtAddr, size) != size)
            {
                sched_process_exit(EEXEC);
            }

            if (!(programHeader.flags & PF_WRITE))
            {
                if (vmm_protect((void*)programHeader.virtAddr, size, PROT_READ) == ERR)
                {
                    sched_process_exit(EEXEC);
                }
            }
        }
        break;
        }
    }

    return (void*)header.entry;
}

void loader_entry(void)
{
    void* rsp = loader_allocate_stack();
    void* rip = loader_load_program();

    loader_jump_to_user_space(rsp, rip);
}
