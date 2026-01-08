#include <kernel/cpu/gdt.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/fs/namespace.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/mem/vmm.h>
#include <kernel/proc/process.h>
#include <kernel/sched/loader.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>

#include <errno.h>
#include <kernel/sched/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/elf.h>
#include <sys/math.h>
#include <sys/proc.h>

static void loader_strv_free(char** array, uint64_t amount)
{
    if (array == NULL)
    {
        return;
    }

    for (uint64_t i = 0; i < amount; i++)
    {
        if (array[i] == NULL)
        {
            continue;
        }
        free(array[i]);
    }
    free((void*)array);
}

void loader_exec(void)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = NULL;
    void* fileData = NULL;

    uintptr_t* addrs = NULL;

    file = vfs_open(PATHNAME(process->argv[0]), process);
    if (file == NULL)
    {
        goto cleanup;
    }

    if (!(file->mode & MODE_EXECUTE))
    {
        errno = EACCES;
        goto cleanup;
    }

    size_t fileSize = vfs_seek(file, 0, SEEK_END);
    vfs_seek(file, 0, SEEK_SET);

    fileData = malloc(fileSize);
    if (fileData == NULL)
    {
        errno = ENOMEM;
        goto cleanup;
    }

    size_t readSize = vfs_read(file, fileData, fileSize);
    if (readSize != fileSize)
    {
        goto cleanup;
    }

    Elf64_File elf;
    if (elf64_validate(&elf, fileData, fileSize) != 0)
    {
        errno = ENOEXEC;
        goto cleanup;
    }

    Elf64_Addr minAddr = UINT64_MAX;
    Elf64_Addr maxAddr = 0;
    elf64_get_loadable_bounds(&elf, &minAddr, &maxAddr);
    uint64_t loadSize = maxAddr - minAddr;

    if (vmm_alloc(&process->space, (void*)minAddr, loadSize, PML_USER | PML_WRITE | PML_PRESENT, VMM_ALLOC_OVERWRITE) ==
        NULL)
    {
        goto cleanup;
    }

    elf64_load_segments(&elf, 0, 0);

    char* rsp = (char*)thread->userStack.top;

    addrs = malloc(sizeof(uintptr_t) * process->argc);
    if (addrs == NULL)
    {
        errno = ENOMEM;
        goto cleanup;
    }

    for (int64_t i = (int64_t)process->argc - 1; i >= 0; i--)
    {
        size_t len = strlen(process->argv[i]) + 1;
        rsp -= len;
        memcpy(rsp, process->argv[i], len);
        addrs[i] = (uintptr_t)rsp;
    }

    rsp = (char*)ROUND_DOWN((uintptr_t)rsp, 8);

    rsp -= (process->argc + 1) * sizeof(char*);
    uintptr_t* argvStack = (uintptr_t*)rsp;
    for (uint64_t i = 0; i < process->argc; i++)
    {
        argvStack[i] = addrs[i];
    }
    argvStack[process->argc] = 0;

    // Disable interrupts, they will be enabled when we jump to user space.
    asm volatile("cli");

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    thread->frame.rsp = ROUND_DOWN((uintptr_t)rsp - sizeof(uint64_t), 16);
    thread->frame.rip = elf.header->e_entry;
    thread->frame.rdi = process->argc;
    thread->frame.rsi = (uintptr_t)rsp;
    thread->frame.cs = GDT_CS_RING3;
    thread->frame.ss = GDT_SS_RING3;
    thread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    errno = EOK;
cleanup:
    if (file != NULL)
    {
        UNREF(file);
    }
    if (fileData != NULL)
    {
        free(fileData);
    }
    if (addrs != NULL)
    {
        free(addrs);
    }
    if (errno == EOK)
    {
        thread_jump(thread);
    }
    LOG_DEBUG("exec failed due to %s\n", strerror(errno));
    sched_process_exit("exec failed");
}

static void loader_entry(void)
{
    thread_t* thread = sched_thread();

    WAIT_BLOCK(&thread->process->suspendQueue, !(atomic_load(&thread->process->flags) & PROCESS_SUSPENDED));

    file_table_close_mode(&thread->process->fileTable, MODE_PRIVATE);

    loader_exec();
}

SYSCALL_DEFINE(SYS_SPAWN, pid_t, const char** argv, spawn_flags_t flags)
{
    if (argv == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    thread_t* thread = sched_thread();
    assert(thread != NULL);
    process_t* process = thread->process;
    assert(process != NULL);

    namespace_t* ns = process_get_ns(process);
    if (ns == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(ns);

    namespace_t* childNs;
    if (flags & SPAWN_EMPTY_NS || flags & SPAWN_COPY_NS)
    {
        childNs = namespace_new(ns);
        if (childNs == NULL)
        {
            return ERR;
        }

        if (!(flags & SPAWN_EMPTY_NS))
        {
            if (namespace_copy(childNs, ns) == ERR)
            {
                UNREF(childNs);
                return ERR;
            }
        }
    }
    else
    {
        childNs = REF(ns);
    }
    UNREF_DEFER(childNs);

    process_t* child =
        process_new(atomic_load(&process->priority), flags & SPAWN_EMPTY_GROUP ? NULL : &process->group, childNs);
    if (child == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(child);

    thread_t* childThread = thread_new(child);
    if (childThread == NULL)
    {
        return ERR;
    }

    char** argvCopy = NULL;
    uint64_t argc = 0;
    if (thread_copy_from_user_string_array(thread, argv, &argvCopy, &argc) == ERR)
    {
        return ERR;
    }

    if (argc == 0 || argvCopy[0] == NULL)
    {
        loader_strv_free(argvCopy, argc);
        errno = EINVAL;
        return ERR;
    }

    if (process_set_cmdline(child, argvCopy, argc) == ERR)
    {
        loader_strv_free(argvCopy, argc);
        return ERR;
    }

    if (flags & SPAWN_SUSPEND)
    {
        atomic_fetch_or(&child->flags, PROCESS_SUSPENDED);
    }

    if (!(flags & SPAWN_EMPTY_FDS))
    {
        if (flags & SPAWN_STDIO_FDS)
        {
            if (file_table_copy(&child->fileTable, &process->fileTable, 0, 3) == ERR)
            {
                loader_strv_free(argvCopy, argc);
                return ERR;
            }
        }
        else
        {
            if (file_table_copy(&child->fileTable, &process->fileTable, 0, CONFIG_MAX_FD) == ERR)
            {
                loader_strv_free(argvCopy, argc);
                return ERR;
            }
        }
    }

    if (!(flags & SPAWN_EMPTY_ENV))
    {
        if (env_copy(&child->env, &process->env) == ERR)
        {
            loader_strv_free(argvCopy, argc);
            return ERR;
        }
    }

    if (!(flags & SPAWN_EMPTY_CWD))
    {
        path_t cwd = cwd_get(&process->cwd, ns);
        cwd_set(&child->cwd, &cwd);
        path_put(&cwd);
    }

    // Call loader_exec()
    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    childThread->frame.rip = (uintptr_t)loader_entry;
    childThread->frame.cs = GDT_CS_RING0;
    childThread->frame.ss = GDT_SS_RING0;
    childThread->frame.rsp = childThread->kernelStack.top;
    childThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    sched_submit(childThread);
    return child->id;
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

    thread_t* newThread = thread_new(process);
    if (newThread == NULL)
    {
        return ERR;
    }

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    newThread->frame.rip = (uint64_t)entry;
    newThread->frame.rsp = newThread->userStack.top;
    newThread->frame.rbp = newThread->userStack.top;
    newThread->frame.rdi = (uint64_t)arg;
    newThread->frame.cs = GDT_CS_RING3;
    newThread->frame.ss = GDT_SS_RING3;
    newThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    tid_t volatile result = newThread->id; // Important to not deref after pushing the thread
    sched_submit(newThread);
    return result;
}