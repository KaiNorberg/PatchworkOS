#include "loader.h"

#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>

#include "cpu/gdt.h"
#include "errno.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/vmm.h"
#include "sched.h"
#include "sched/thread.h"
#include "stdarg.h"

static void* loader_load_program(thread_t* thread)
{
    process_t* process = thread->process;
    space_t* space = &process->space;

    const char* executable = process->argv.buffer[0];
    if (executable == NULL)
    {
        sched_process_exit(ESPAWNFAIL);
    }

    file_t* file = vfs_open(PATHNAME(executable));
    if (file == NULL)
    {
        sched_process_exit(ESPAWNFAIL);
    }
    FILE_DEFER(file);

    elf_hdr_t header;
    if (vfs_read(file, &header, sizeof(elf_hdr_t)) != sizeof(elf_hdr_t))
    {
        sched_process_exit(ESPAWNFAIL);
    }
    if (!ELF_IS_VALID(&header))
    {
        sched_process_exit(ESPAWNFAIL);
    }

    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    for (uint64_t i = 0; i < header.phdrAmount; i++)
    {
        uint64_t offset = sizeof(elf_hdr_t) + header.phdrSize * i;
        if (vfs_seek(file, offset, SEEK_SET) != offset)
        {
            sched_process_exit(ESPAWNFAIL);
        }

        elf_phdr_t phdr;
        if (vfs_read(file, &phdr, sizeof(elf_phdr_t)) != sizeof(elf_phdr_t))
        {
            sched_process_exit(ESPAWNFAIL);
        }

        switch (phdr.type)
        {
        case ELF_PHDR_TYPE_LOAD:
        {
            min = MIN(min, phdr.virtAddr);
            max = MAX(max, phdr.virtAddr + phdr.memorySize);
            if (phdr.memorySize < phdr.fileSize)
            {
                sched_process_exit(ESPAWNFAIL);
            }

            if (vmm_alloc(space, (void*)phdr.virtAddr, phdr.memorySize, PROT_READ | PROT_WRITE) == NULL)
            {
                sched_process_exit(ESPAWNFAIL);
            }
            memset((void*)phdr.virtAddr, 0, phdr.memorySize);

            if (vfs_seek(file, phdr.offset, SEEK_SET) != phdr.offset)
            {
                sched_process_exit(ESPAWNFAIL);
            }
            if (vfs_read(file, (void*)phdr.virtAddr, phdr.fileSize) != phdr.fileSize)
            {
                sched_process_exit(ESPAWNFAIL);
            }

            if (!(phdr.flags & ELF_PHDR_FLAGS_WRITE))
            {
                if (vmm_protect(space, (void*)phdr.virtAddr, phdr.memorySize, PROT_READ) == ERR)
                {
                    sched_process_exit(ESPAWNFAIL);
                }
            }
        }
        break;
        }
    }

    return (void*)header.entry;
}

static void* loader_alloc_user_stack(thread_t* thread)
{
    uintptr_t stackTop = LOADER_USER_STACK_TOP(thread->id);

    if (vmm_alloc(&thread->process->space, (void*)(stackTop - PAGE_SIZE), PAGE_SIZE, PROT_READ | PROT_WRITE) == NULL)
    {
        sched_process_exit(ESPAWNFAIL);
    }

    return (void*)stackTop;
}

static char** loader_setup_argv(thread_t* thread, void* rsp)
{
    if (thread->process->argv.size >= PAGE_SIZE)
    {
        sched_process_exit(ESPAWNFAIL);
    }

    char** argv = memcpy(rsp - sizeof(uint64_t) - thread->process->argv.size, thread->process->argv.buffer,
        thread->process->argv.size);

    for (uint64_t i = 0; i < thread->process->argv.amount; i++)
    {
        argv[i] = (void*)((uint64_t)argv[i] - (uint64_t)thread->process->argv.buffer + (uint64_t)argv);
    }

    return argv;
}

static void loader_process_entry(void)
{
    thread_t* thread = sched_thread();

    void* rsp = loader_alloc_user_stack(thread);
    void* rip = loader_load_program(thread);
    char** argv = loader_setup_argv(thread, rsp);
    rsp = (void*)ROUND_DOWN((uint64_t)argv - 1, 16);

    loader_jump_to_user_space(thread->process->argv.amount, argv, rsp, rip);
}

thread_t* loader_spawn(const char** argv, priority_t priority, const path_t* cwd)
{
    process_t* process = sched_process();

    if (argv == NULL || argv[0] == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    pathname_t executable;
    if (pathname_init(&executable, argv[0]) == ERR)
    {
        return NULL;
    }

    stat_t info;
    if (vfs_stat(&executable, &info) == ERR)
    {
        return NULL;
    }

    if (info.type != INODE_FILE)
    {
        errno = EISDIR;
        return NULL;
    }

    process_t* child = process_new(process, argv, cwd, priority);
    if (child == NULL)
    {
        return NULL;
    }

    thread_t* childThread = thread_new(child, loader_process_entry);
    if (childThread == NULL)
    {
        process_free(child);
        return NULL;
    }

    LOG_INFO("loader: spawn path=%s pid=%d\n", argv[0], child->id);
    return childThread;
}

thread_t* loader_thread_create(process_t* parent, void* entry, void* arg)
{
    thread_t* child = thread_new(parent, entry);
    if (child == NULL)
    {
        return NULL;
    }

    void* rsp = loader_alloc_user_stack(child);
    if (rsp == NULL)
    {
        thread_free(child);
        return NULL;
    }

    child->trapFrame.cs = GDT_USER_CODE | GDT_RING3;
    child->trapFrame.ss = GDT_USER_DATA | GDT_RING3;
    child->trapFrame.rsp = (uint64_t)rsp;
    child->trapFrame.rdi = (uint64_t)arg;
    return child;
}
