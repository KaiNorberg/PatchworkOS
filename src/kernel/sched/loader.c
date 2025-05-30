#include "loader.h"

#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>

#include "cpu/gdt.h"
#include "errno.h"
#include "fs/vfs.h"
#include "mem/vmm.h"
#include "sched/thread.h"
#include "sched.h"
#include "stdarg.h"
#include "utils/log.h"

#include <stdio.h>

static void* loader_load_program(thread_t* thread)
{
    process_t* process = thread->process;
    space_t* space = &process->space;

    const char* executable = process->argv.buffer[0];
    if (executable == NULL)
    {
        printf("loader_load_program: executable == NULL (%s) pid=%d\n", strerror(thread->error), process->id);
        sched_process_exit(EEXEC);
    }

    file_t* file = vfs_open(PATH(process, executable));
    if (file == NULL)
    {
        printf("loader_load_program: vfs_open failed (%s) pid=%d\n", strerror(thread->error), process->id);
        sched_process_exit(EEXEC);
    }
    FILE_DEFER(file);

    elf_hdr_t header;
    if (vfs_read(file, &header, sizeof(elf_hdr_t)) != sizeof(elf_hdr_t))
    {
        printf("loader_load_program: vfs_read hdr failed (%s) pid=%d\n", strerror(thread->error), process->id);
        sched_process_exit(EEXEC);
    }
    if (!ELF_IS_VALID(&header))
    {
        printf("loader_load_program: elf valid check failed (%s) pid=%d\n", strerror(thread->error),
            process->id);
        sched_process_exit(EEXEC);
    }

    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    for (uint64_t i = 0; i < header.phdrAmount; i++)
    {
        uint64_t offset = sizeof(elf_hdr_t) + header.phdrSize * i;
        if (vfs_seek(file, offset, SEEK_SET) != offset)
        {
            printf("loader_load_program: vfs_seek to offset failed (%s) pid=%d\n", strerror(thread->error),
                process->id);
            sched_process_exit(EEXEC);
        }

        elf_phdr_t phdr;
        if (vfs_read(file, &phdr, sizeof(elf_phdr_t)) != sizeof(elf_phdr_t))
        {
            printf("loader_load_program: vfs_read phdr failed (%s) pid=%d\n", strerror(thread->error),
                process->id);
            sched_process_exit(EEXEC);
        }

        switch (phdr.type)
        {
        case ELF_PHDR_TYPE_LOAD:
        {
            min = MIN(min, phdr.virtAddr);
            max = MAX(max, phdr.virtAddr + phdr.memorySize);
            if (phdr.memorySize < phdr.fileSize)
            {
                printf("loader_load_program: phdr size check failed (%s) pid=%d\n", strerror(thread->error),
                    process->id);
                sched_process_exit(EEXEC);
            }

            if (vmm_alloc(space, (void*)phdr.virtAddr, phdr.memorySize, PROT_READ | PROT_WRITE) == NULL)
            {
                printf("loader_load_program: vmm_alloc failed (%s) pid=%d\n", strerror(thread->error),
                    process->id);
                sched_process_exit(EEXEC);
            }
            memset((void*)phdr.virtAddr, 0, phdr.memorySize);

            if (vfs_seek(file, phdr.offset, SEEK_SET) != phdr.offset)
            {
                printf("loader_load_program: vfs_seek failed (%s) pid=%d\n", strerror(thread->error),
                    process->id);
                sched_process_exit(EEXEC);
            }
            if (vfs_read(file, (void*)phdr.virtAddr, phdr.fileSize) != phdr.fileSize)
            {
                printf("loader_load_program: vfs_read failed (%s) pid=%d\n", strerror(thread->error),
                    process->id);
                sched_process_exit(EEXEC);
            }

            if (!(phdr.flags & ELF_PHDR_FLAGS_WRITE))
            {
                if (vmm_protect(space, (void*)phdr.virtAddr, phdr.memorySize, PROT_READ) == ERR)
                {
                    printf("loader_load_program: vmm_protect failed (%s) pid=%d\n", strerror(thread->error),
                        process->id);
                    sched_process_exit(EEXEC);
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

    if (vmm_alloc(&thread->process->space, (void*)(stackTop - PAGE_SIZE), PAGE_SIZE,
            PROT_READ | PROT_WRITE) == NULL)
    {
        printf("loader_alloc_user_stack: vmm_alloc failed (%s) pid=%d\n", strerror(thread->error), thread->process->id);
        sched_process_exit(EEXEC);
    }

    return (void*)stackTop;
}

static char** loader_setup_argv(thread_t* thread, void* rsp)
{
    if (thread->process->argv.size >= PAGE_SIZE)
    {
        sched_process_exit(EEXEC);
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
        return ERRPTR(EINVAL);
    }

    stat_t info;
    if (vfs_stat(PATH(process, argv[0]), &info) == ERR)
    {
        return NULL;
    }

    if (info.type != STAT_FILE)
    {
        return ERRPTR(EISDIR);
    }

    process_t* child = process_new(process, argv, cwd);
    if (child == NULL)
    {
        return NULL;
    }

    thread_t* childThread = thread_new(child, loader_process_entry, priority);
    if (childThread == NULL)
    {
        process_free(child);
        return NULL;
    }

    printf("loader: spawn path=%s pid=%d\n", argv[0], child->id);
    return childThread;
}

thread_t* loader_thread_create(process_t* parent, priority_t priority, void* entry, void* arg)
{
    thread_t* child = thread_new(parent, entry, priority);
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