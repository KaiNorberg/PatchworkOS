#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
#include <kernel/sched/loader.h>

#include <kernel/cpu/gdt.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>

#include <errno.h>
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

void loader_exec(const char* executable, char** argv, uint64_t argc)
{
    assert(executable != NULL);
    assert((argv != NULL && argc > 0) || ((argv == NULL || argv[0] == NULL) && argc == 0));

    // Generic error if a lower function fails without setting errno
    errno = ESPAWNFAIL;

    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    file_t* file = NULL;
    void* fileData = NULL;

    file = vfs_open(PATHNAME(executable), process);
    if (file == NULL)
    {
        goto cleanup;
    }

    uint64_t fileSize = vfs_seek(file, 0, SEEK_END);
    vfs_seek(file, 0, SEEK_SET);

    fileData = malloc(fileSize);
    if (fileData == NULL)
    {
        goto cleanup;
    }

    uint64_t readSize = vfs_read(file, fileData, fileSize);
    if (readSize != fileSize)
    {
        goto cleanup;
    }

    Elf64_File elf;
    if (elf64_validate(&elf, fileData, fileSize) != 0)
    {
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
    for (int64_t i = argc - 1; i >= 0; i--)
    {
        size_t len = strlen(argv[i]) + 1;
        rsp -= len;
        memcpy(rsp, argv[i], len);

        free(argv[i]);
        argv[i] = rsp;
    }
    rsp -= sizeof(char*);
    *((char**)rsp) = NULL;
    for (int64_t i = argc - 1; i >= 0; i--)
    {
        rsp -= sizeof(char*);
        *((char**)rsp) = argv[i];
        argv[i] = NULL;
    }

    // Disable interrupts, they will be enabled when we jump to user space.
    asm volatile("cli");

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    thread->frame.rsp = ROUND_DOWN((uintptr_t)rsp - sizeof(uint64_t), 16);
    thread->frame.rip = elf.header->e_entry;
    thread->frame.rdi = argc;
    thread->frame.rsi = (uintptr_t)rsp;
    thread->frame.cs = GDT_CS_RING3;
    thread->frame.ss = GDT_SS_RING3;
    thread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    errno = EOK;
cleanup:
    if (file != NULL)
    {
        DEREF(file);
    }
    if (fileData != NULL)
    {
        free(fileData);
    }
    free((void*)executable);
    loader_strv_free(argv, argc);
    if (errno == EOK)
    {
        thread_jump(thread);
    }
    sched_process_exit(errno);
}

SYSCALL_DEFINE(SYS_SPAWN, pid_t, const char** argv, const spawn_fd_t* fds, const char* cwd, priority_t priority,
    spawn_flags_t flags)
{
    process_t* child = NULL;
    thread_t* childThread = NULL;

    uint64_t argc = 0;
    char** argvCopy = NULL;
    char* executable = NULL;

    if (argv == NULL || (priority > PRIORITY_MAX_USER && priority != PRIORITY_PARENT))
    {
        errno = EINVAL;
        goto error;
    }

    thread_t* thread = sched_thread();
    process_t* process = thread->process;

    if (priority == PRIORITY_PARENT)
    {
        priority = atomic_load(&process->priority);
    }

    child = process_new(priority);
    if (child == NULL)
    {
        goto error;
    }

    if (fds != NULL)
    {
        spawn_fd_t* fdsCopy;
        uint64_t fdAmount;
        spawn_fd_t fdsTerminator = SPAWN_FD_END;
        if (thread_copy_from_user_terminated(thread, fds, &fdsTerminator, sizeof(spawn_fd_t), CONFIG_MAX_FD,
                (void**)&fdsCopy, &fdAmount) == ERR)
        {
            goto error;
        }

        for (uint64_t i = 0; i < fdAmount; i++)
        {
            file_t* file = file_table_get(&process->fileTable, fdsCopy[i].parent);
            if (file == NULL)
            {
                free(fdsCopy);
                goto error;
            }

            if (file_table_set(&child->fileTable, fdsCopy[i].child, file) == ERR)
            {
                free(fdsCopy);
                DEREF(file);
                goto error;
            }
            DEREF(file);
        }
        free(fdsCopy);
    }

    if (cwd != NULL)
    {
        pathname_t cwdPathname;
        if (thread_copy_from_user_pathname(thread, &cwdPathname, cwd) == ERR)
        {
            goto error;
        }

        path_t cwdParent = cwd_get(&process->cwd);
        PATH_DEFER(&cwdParent);

        path_t cwdPath = PATH_EMPTY;
        if (path_walk(&cwdPath, &cwdPathname, &cwdParent, &process->ns) == ERR)
        {
            goto error;
        }

        if (!dentry_is_positive(cwdPath.dentry))
        {
            path_put(&cwdPath);
            goto error;
        }

        cwd_set(&child->cwd, &cwdPath);
        path_put(&cwdPath);
    }
    else
    {
        path_t cwdParent = cwd_get(&process->cwd);
        cwd_set(&child->cwd, &cwdParent);
        path_put(&cwdParent);
    }

    childThread = thread_new(child);
    if (childThread == NULL)
    {
        goto error;
    }

    if (thread_copy_from_user_string_array(thread, argv, &argvCopy, &argc) == ERR)
    {
        goto error;
    }

    if (argc == 0 || argvCopy[0] == NULL)
    {
        goto error;
    }

    executable = strdup(argvCopy[0]);
    if (executable == NULL)
    {
        goto error;
    }

    if (!(flags & SPAWN_EMPTY_NAMESPACE))
    {
        if (namespace_set_parent(&child->ns, &process->ns) == ERR)
        {
            goto error;
        }
    }

    if (!(flags & SPAWN_EMPTY_ENVIRONMENT))
    {
        if (process_copy_env(child, process) == ERR)
        {
            goto error;
        }
    }

    // Call loader_exec(executable, argvCopy, argc)
    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    childThread->frame.rip = (uintptr_t)loader_exec;
    childThread->frame.rdi = (uintptr_t)executable;
    childThread->frame.rsi = (uintptr_t)argvCopy;
    childThread->frame.rdx = argc;

    childThread->frame.cs = GDT_CS_RING0;
    childThread->frame.ss = GDT_SS_RING0;
    childThread->frame.rsp = childThread->kernelStack.top;
    childThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    pid_t volatile result = child->id; // Important to not deref after pushing the thread
    sched_submit(childThread);
    return result;

error:
    if (childThread != NULL)
    {
        thread_free(childThread);
    }
    if (child != NULL)
    {
        process_kill(child, errno);
    }

    free((void*)executable);
    loader_strv_free(argvCopy, argc);
    return ERR;
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