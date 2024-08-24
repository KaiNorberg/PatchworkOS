#include "loader.h"

#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>

#include "errno.h"
#include "gdt.h"
#include "log.h"
#include "sched.h"
#include "thread.h"
#include "vfs.h"
#include "vmm.h"

static void* loader_load_program(thread_t* thread)
{
    const char* executable = sched_process()->argv[0];
    if (executable == NULL)
    {
        return NULL;
    }

    file_t* file = vfs_open(executable);
    if (file == NULL)
    {
        return NULL;
    }
    FILE_DEFER(file);

    char parentDir[MAX_PATH];
    vfs_parent_dir(parentDir, executable);
    if (vfs_chdir(parentDir) == ERR)
    {
        return NULL;
    }

    elf_hdr_t header;
    if (vfs_read(file, &header, sizeof(elf_hdr_t)) != sizeof(elf_hdr_t))
    {
        return NULL;
    }
    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' || header.ident[3] != 'F')
    {
        return NULL;
    }

    for (uint64_t i = 0; i < header.programHeaderAmount; i++)
    {
        uint64_t offset = sizeof(elf_hdr_t) + header.programHeaderSize * i;
        if (vfs_seek(file, offset, SEEK_SET) != offset)
        {
            return NULL;
        }

        elf_phdr_t programHeader;
        if (vfs_read(file, &programHeader, sizeof(elf_phdr_t)) != sizeof(elf_phdr_t))
        {
            return NULL;
        }

        switch (programHeader.type)
        {
        case PT_LOAD:
        {
            uint64_t size = MAX(programHeader.memorySize, programHeader.fileSize);

            if (vmm_alloc((void*)programHeader.virtAddr, size, PROT_READ | PROT_WRITE) == NULL)
            {
                return NULL;
            }

            if (vfs_seek(file, programHeader.offset, SEEK_SET) != programHeader.offset)
            {
                return NULL;
            }

            memset((void*)programHeader.virtAddr, 0, size);
            if (vfs_read(file, (void*)programHeader.virtAddr, size) != size)
            {
                return NULL;
            }

            if (!(programHeader.flags & PF_WRITE))
            {
                if (vmm_protect((void*)programHeader.virtAddr, size, PROT_READ) == ERR)
                {
                    return NULL;
                }
            }
        }
        break;
        }
    }

    return (void*)header.entry;
}

static void* loader_allocate_stack(thread_t* thread)
{
    void* address = (void*)(VMM_LOWER_HALF_MAX - (CONFIG_USER_STACK * (thread->id + 1) + PAGE_SIZE * (thread->id)));
    if (vmm_alloc(address, CONFIG_USER_STACK, PROT_READ | PROT_WRITE) == NULL)
    {
        return NULL;
    }

    return address + CONFIG_USER_STACK;
}

static void loader_spawn_entry(void)
{
    thread_t* thread = sched_thread();

    void* rsp = loader_allocate_stack(thread);
    if (rsp == NULL)
    {
        log_print("loader: stack failure (%s, %d)", thread->process->argv[0], thread->process->id);
        sched_process_exit(EEXEC);
    }

    void* rip = loader_load_program(thread);
    if (rip == NULL)
    {
        log_print("loader: load failure (%s, %d)", thread->process->argv[0], thread->process->id);
        sched_process_exit(EEXEC);
    }

    loader_jump_to_user_space(rsp, rip);
}

thread_t* loader_spawn(const char** argv, priority_t priority)
{
    if (argv == NULL || argv[0] == NULL)
    {
        return ERRPTR(EINVAL);
    }

    char executable[MAX_PATH];
    stat_t info;
    if (vfs_realpath(executable, argv[0]) == ERR || vfs_stat(executable, &info) == ERR)
    {
        return NULL;
    }

    if (info.type != STAT_FILE)
    {
        return ERRPTR(EISDIR);
    }

    thread_t* thread = thread_new(argv, loader_spawn_entry, priority);
    if (thread == NULL)
    {
        return NULL;
    }

    return thread;
}

thread_t* loader_split(thread_t* thread, void* entry, priority_t priority)
{
    thread_t* child = thread_split(thread, entry, priority);
    if (child == NULL)
    {
        return NULL;
    }

    void* rsp = loader_allocate_stack(child);
    if (rsp == NULL)
    {
        thread_free(child);
        return NULL;
    }

    child->trapFrame.cs = GDT_USER_CODE | GDT_RING3;
    child->trapFrame.ss = GDT_USER_DATA | GDT_RING3;
    child->trapFrame.rsp = (uint64_t)rsp;

    return child;
}
