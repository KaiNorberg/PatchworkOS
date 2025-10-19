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

static void* loader_load_program(thread_t* thread)
{
    process_t* process = thread->process;
    space_t* space = &process->space;

    const char* executable = process->argv.buffer[0];
    if (executable == NULL)
    {
        return NULL;
    }

    file_t* file = vfs_open(PATHNAME(executable));
    if (file == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(file);

    elf_hdr_t header;
    if (vfs_read(file, &header, sizeof(elf_hdr_t)) != sizeof(elf_hdr_t))
    {
        return NULL;
    }
    if (!ELF_IS_VALID(&header))
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
            if (phdr.memorySize < phdr.fileSize)
            {
                return NULL;
            }

            if (vmm_alloc(space, (void*)phdr.virtAddr, phdr.memorySize, PML_PRESENT | PML_WRITE | PML_USER) == NULL)
            {
                return NULL;
            }
            memset((void*)phdr.virtAddr, 0, phdr.memorySize);

            if (vfs_seek(file, phdr.offset, SEEK_SET) != phdr.offset)
            {
                return NULL;
            }
            if (vfs_read(file, (void*)phdr.virtAddr, phdr.fileSize) != phdr.fileSize)
            {
                return NULL;
            }

            if (!(phdr.flags & ELF_PHDR_FLAGS_WRITE))
            {
                if (vmm_protect(&thread->process->space, (void*)phdr.virtAddr, phdr.memorySize,
                        PML_PRESENT | PML_USER) == ERR)
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

static char** loader_setup_argv(thread_t* thread)
{
    if (thread->process->argv.size >= PAGE_SIZE)
    {
        return NULL;
    }

    char** argv = memcpy((void*)(thread->userStack.top - sizeof(uint64_t) - thread->process->argv.size),
        thread->process->argv.buffer, thread->process->argv.size);

    for (uint64_t i = 0; i < thread->process->argv.amount; i++)
    {
        argv[i] = (void*)((uint64_t)argv[i] - (uint64_t)thread->process->argv.buffer + (uint64_t)argv);
    }

    return argv;
}

static void loader_process_entry(void)
{
    thread_t* thread = sched_thread();

    void* rip = loader_load_program(thread);
    if (rip == NULL)
    {
        sched_process_exit(ESPAWNFAIL);
    }

    char** argv = loader_setup_argv(thread);
    if (argv == NULL)
    {
        sched_process_exit(ESPAWNFAIL);
    }

    // Disable interrupts, they will be enabled by the IRETQ instruction.
    asm volatile("cli");

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    thread->frame.rdi = thread->process->argv.amount;
    thread->frame.rsi = (uintptr_t)argv;
    thread->frame.rsp = (uintptr_t)ROUND_DOWN((uint64_t)argv - 1, 16);
    thread->frame.rip = (uintptr_t)rip;
    thread->frame.cs = GDT_CS_RING3;
    thread->frame.ss = GDT_SS_RING3;
    thread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    LOG_DEBUG("jump to user space path=%s pid=%d rsp=%p rip=%p\n", thread->process->argv.buffer[0], thread->process->id,
        (void*)thread->frame.rsp, (void*)thread->frame.rip);
    loader_jump_to_user_space(thread);
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
    DEREF_DEFER(child);

    thread_t* childThread = thread_new(child);
    if (childThread == NULL)
    {
        return NULL;
    }

    childThread->frame.rip = (uintptr_t)loader_process_entry;
    childThread->frame.rsp = childThread->kernelStack.top;
    childThread->frame.cs = GDT_CS_RING0;
    childThread->frame.ss = GDT_SS_RING0;
    childThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    LOG_INFO("spawn path=%s pid=%d\n", argv[0], child->id);
    return childThread;
}

SYSCALL_DEFINE(SYS_SPAWN, pid_t, const char** argv, const spawn_fd_t* fds, const char* cwdString, spawn_attr_t* attr)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;
    space_t* space = &process->space;

    priority_t priority;
    if (attr == NULL || attr->flags & SPAWN_INHERIT_PRIORITY)
    {
        priority = atomic_load(&process->priority);
    }
    else
    {
        priority = attr->priority;
    }

    if (priority >= PRIORITY_MAX_USER)
    {
        errno = EACCES;
        return ERR;
    }

    char* argvTerminator = NULL;
    uint64_t argvSize = space_pin_terminated(space, argv, &argvTerminator, sizeof(char*), CONFIG_MAX_ARGC);
    if (argvSize == ERR)
    {
        return ERR;
    }
    uint64_t argc = argvSize / sizeof(char*);

    // TODO: This is not safe against TOCTOU attacks. We need to copy the entire argv array and all strings.

    thread_t* child;
    if (cwdString == NULL)
    {
        child = loader_spawn(argv, priority, NULL);
    }
    else
    {
        pathname_t cwdPathname;
        if (space_safe_pathname_init(space, &cwdPathname, cwdString) == ERR)
        {
            space_unpin(space, argv, argvSize);
            return ERR;
        }

        path_t cwdPath = PATH_EMPTY;
        if (vfs_walk(&cwdPath, &cwdPathname, WALK_NONE) == ERR)
        {
            space_unpin(space, argv, argvSize);
            return ERR;
        }

        child = loader_spawn(argv, priority, &cwdPath);

        path_put(&cwdPath);
    }
    space_unpin(space, argv, argvSize);

    if (child == NULL)
    {
        return ERR;
    }

    spawn_fd_t fdsTerminator = SPAWN_FD_END;
    uint64_t fdsSize = space_pin_terminated(space, fds, &fdsTerminator, sizeof(spawn_fd_t), CONFIG_MAX_FD);
    if (fdsSize == ERR)
    {
        DEREF(child);
        return ERR;
    }
    uint64_t fdAmount = fdsSize / sizeof(spawn_fd_t);

    vfs_ctx_t* parentVfsCtx = &process->vfsCtx;
    vfs_ctx_t* childVfsCtx = &child->process->vfsCtx;

    for (uint64_t i = 0; i < fdAmount; i++)
    {
        file_t* file = vfs_ctx_get_file(parentVfsCtx, fds[i].parent);
        if (file == NULL)
        {
            thread_free(child);
            space_unpin(space, fds, fdsSize);
            errno = EBADF;
            return ERR;
        }
        DEREF_DEFER(file);

        if (vfs_ctx_openas(childVfsCtx, fds[i].child, file) == ERR)
        {
            thread_free(child);
            space_unpin(space, fds, fdsSize);
            errno = EBADF;
            return ERR;
        }
    }
    space_unpin(space, fds, fdsSize);

    sched_push_new_thread(child, thread);
    return child->process->id;
}

thread_t* loader_thread_create(process_t* parent, void* entry, void* arg)
{
    thread_t* child = thread_new(parent);
    if (child == NULL)
    {
        return NULL;
    }

    child->frame.rip = (uint64_t)entry;
    child->frame.rsp = child->userStack.top;
    child->frame.rdi = (uint64_t)arg;
    child->frame.cs = GDT_CS_RING3;
    child->frame.ss = GDT_SS_RING3;
    child->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;
    return child;
}

SYSCALL_DEFINE(SYS_THREAD_CREATE, tid_t, void* entry, void* arg)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;
    space_t* space = &process->space;

    if (space_check_access(space, entry, sizeof(uint64_t)) == ERR)
    {
        return ERR;
    }

    // Dont check arg user space can use it however it wants

    thread_t* newThread = loader_thread_create(process, entry, arg);
    if (newThread == NULL)
    {
        return ERR;
    }

    sched_push_new_thread(newThread, thread);
    return newThread->id;
}
