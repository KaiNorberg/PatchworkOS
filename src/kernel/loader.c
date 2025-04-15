#include "loader.h"

#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>

#include "errno.h"
#include "gdt.h"
#include "log.h"
#include "sched.h"
#include "stdarg.h"
#include "thread.h"
#include "vfs.h"
#include "vmm.h"

#include <stdio.h>

static void* loader_load_program(thread_t* thread)
{
    const char* executable = sched_process()->argv.buffer[0];
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

    elf_hdr_t header;
    if (vfs_read(file, &header, sizeof(elf_hdr_t)) != sizeof(elf_hdr_t))
    {
        return NULL;
    }
    if (!ELF_VALID_CHECK(&header))
    {
        return NULL;
    }

    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    for (uint64_t i = 0; i < header.phdrAmount; i++)
    {
        uint64_t offset = sizeof(elf_hdr_t) + header.phdrSize * i;
        if (vfs_seek(file, offset, SEEK_SET) != offset)
        {
            return NULL;
        }

        elf_phdr_t phdr;
        if (vfs_read(file, &phdr, sizeof(elf_phdr_t)) != sizeof(elf_phdr_t))
        {
            return NULL;
        }

        switch (phdr.type)
        {
        case ELF_PHDR_TYPE_LOAD:
        {
            min = MIN(min, phdr.virtAddr);
            max = MAX(max, phdr.virtAddr + phdr.memorySize);
            uint64_t size = MAX(phdr.memorySize, phdr.fileSize);

            if (vmm_alloc((void*)phdr.virtAddr, size, PROT_READ | PROT_WRITE) == NULL)
            {
                return NULL;
            }

            if (vfs_seek(file, phdr.offset, SEEK_SET) != phdr.offset)
            {
                return NULL;
            }

            memset((void*)phdr.virtAddr, 0, size);
            if (vfs_read(file, (void*)phdr.virtAddr, size) != size)
            {
                return NULL;
            }

            if (!(phdr.flags & ELF_PHDR_FLAGS_WRITE))
            {
                if (vmm_protect((void*)phdr.virtAddr, size, PROT_READ) == ERR)
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
    uintptr_t base = LOADER_STACK_ADDRESS(thread->id);
    if (vmm_alloc((void*)(base + PAGE_SIZE), CONFIG_USER_STACK, PROT_READ | PROT_WRITE) == NULL)
    {
        return NULL;
    }

    return (void*)(base + PAGE_SIZE + CONFIG_USER_STACK);
}

static char** loader_setup_argv(thread_t* thread, void* rsp)
{
    char** argv =
        memcpy(rsp - sizeof(uint64_t) - thread->process->argv.size, thread->process->argv.buffer, thread->process->argv.size);

    for (uint64_t i = 0; i < thread->process->argv.amount; i++)
    {
        argv[i] = (void*)((uint64_t)argv[i] - (uint64_t)thread->process->argv.buffer + (uint64_t)argv);
    }

    return argv;
}

static void loader_process_entry(void)
{
    thread_t* thread = sched_thread();

    void* rsp = loader_allocate_stack(thread);
    if (rsp == NULL)
    {
        printf("loader: allocate_stack failure pid=%d", thread->process->id);
        sched_process_exit(EEXEC);
    }

    void* rip = loader_load_program(thread);
    if (rip == NULL)
    {
        printf("loader: load_program failure pid=%d", thread->process->id);
        sched_process_exit(EEXEC);
    }

    char** argv = loader_setup_argv(thread, rsp);
    rsp = (void*)ROUND_DOWN((uint64_t)argv - 1, 16);

    loader_jump_to_user_space(thread->process->argv.amount, argv, rsp, rip);
}

thread_t* loader_spawn(const char** argv, priority_t priority)
{
    if (argv == NULL || argv[0] == NULL)
    {
        return ERRPTR(EINVAL);
    }

    stat_t info;
    if (vfs_stat(argv[0], &info) == ERR)
    {
        return NULL;
    }

    if (info.type != STAT_FILE)
    {
        return ERRPTR(EISDIR);
    }

    vfs_ctx_t* ctx = &sched_thread()->process->vfsCtx;
    LOCK_DEFER(&ctx->lock);

    thread_t* thread = thread_new(argv, loader_process_entry, priority, &ctx->cwd);
    if (thread == NULL)
    {
        return NULL;
    }

    printf("loader: spawn path=%s pid=%d", argv[0], thread->process->id);
    return thread;
}

thread_t* loader_thread_create(thread_t* thread, void* entry, priority_t priority, uint64_t argc, va_list args)
{
    if (argc > LOADER_THREAD_MAX_ARGS)
    {
        return ERRPTR(EINVAL);
    }

    thread_t* child = thread_new_inherit(thread, entry, priority);
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
    child->trapFrame.rbp = (uint64_t)rsp;

    if (argc >= 1)
    {
        child->trapFrame.rdi = va_arg(args, uint64_t);
    }
    if (argc >= 2)
    {
        child->trapFrame.rsi = va_arg(args, uint64_t);
    }
    if (argc >= 3)
    {
        child->trapFrame.rdx = va_arg(args, uint64_t);
    }
    if (argc >= 4)
    {
        child->trapFrame.rcx = va_arg(args, uint64_t);
    }

    return child;
}
