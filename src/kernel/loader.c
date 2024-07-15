#include "loader.h"

#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>

#include "errno.h"
#include "log.h"
#include "sched.h"
#include "vfs.h"
#include "vmm.h"

NORETURN static void loader_error(file_t* file)
{
    if (file != NULL)
    {
        file_deref(file);
    }

    log_print("loader: failure (%s, %d)", sched_process()->executable, sched_process()->id);
    sched_process_exit(EEXEC);
}

static void* loader_allocate_stack(void)
{
    thread_t* thread = sched_thread();

    void* address = (void*)(VMM_LOWER_HALF_MAX - (CONFIG_USER_STACK * (thread->id + 1) + PAGE_SIZE * (thread->id)));
    if (vmm_alloc(address, CONFIG_USER_STACK, PROT_READ | PROT_WRITE) == NULL)
    {
        loader_error(NULL);
    }

    return address + CONFIG_USER_STACK;
}

static void* loader_load_program(void)
{
    const char* executable = sched_process()->executable;
    file_t* file = vfs_open(executable);
    if (file == NULL)
    {
        loader_error(NULL);
    }

    char parentDir[MAX_PATH];
    vfs_parent_dir(parentDir, executable);
    if (vfs_chdir(parentDir) == ERR)
    {
        loader_error(file);
    }

    elf_hdr_t header;
    if (vfs_read(file, &header, sizeof(elf_hdr_t)) != sizeof(elf_hdr_t))
    {
        loader_error(file);
    }
    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F')
    {
        loader_error(file);
    }

    for (uint64_t i = 0; i < header.programHeaderAmount; i++)
    {
        uint64_t offset = sizeof(elf_hdr_t) + header.programHeaderSize * i;
        if (vfs_seek(file, offset, SEEK_SET) != offset)
        {
            loader_error(file);
        }

        elf_phdr_t programHeader;
        if (vfs_read(file, &programHeader, sizeof(elf_phdr_t)) != sizeof(elf_phdr_t))
        {
            loader_error(file);
        }

        switch (programHeader.type)
        {
        case PT_LOAD:
        {
            uint64_t size = MAX(programHeader.memorySize, programHeader.fileSize);

            if (vmm_alloc((void*)programHeader.virtAddr, size, PROT_READ | PROT_WRITE) == NULL)
            {
                loader_error(file);
            }

            if (vfs_seek(file, programHeader.offset, SEEK_SET) != programHeader.offset)
            {
                loader_error(file);
            }

            memset((void*)programHeader.virtAddr, 0, size);
            if (vfs_read(file, (void*)programHeader.virtAddr, size) != size)
            {
                loader_error(file);
            }

            if (!(programHeader.flags & PF_WRITE))
            {
                if (vmm_protect((void*)programHeader.virtAddr, size, PROT_READ) == ERR)
                {
                    loader_error(file);
                }
            }
        }
        break;
        }
    }

    file_deref(file);
    return (void*)header.entry;
}

void loader_entry(void)
{
    // log_print("loader: loading (%d)", sched_process()->id);

    void* rsp = loader_allocate_stack();
    void* rip = loader_load_program();

    // log_print("loader: loaded (%d)", sched_process()->id);
    loader_jump_to_user_space(rsp, rip);
}
