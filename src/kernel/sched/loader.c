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
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    file_t* file = NULL;
    void* fileData = NULL;

    uintptr_t* addrs = NULL;

    pathname_t pathname;
    status_t status = pathname_init(&pathname, process->argv[0]);
    if (IS_ERR(status))
    {
        goto cleanup;
    }

    status = vfs_open(&file, &pathname, process);
    if (IS_ERR(status))
    {
        goto cleanup;
    }

    if (!(file->mode & MODE_EXECUTE))
    {
        status = ERR(SCHED, PERM);
        goto cleanup;
    }

    size_t fileSize;
    vfs_seek(file, 0, SEEK_END, &fileSize);
    vfs_seek(file, 0, SEEK_SET, NULL);

    fileData = malloc(fileSize);
    if (fileData == NULL)
    {
        status = ERR(SCHED, NOMEM);
        goto cleanup;
    }

    size_t readSize;
    status = vfs_read(file, fileData, fileSize, &readSize);
    if (IS_ERR(status))
    {
        goto cleanup;
    }

    if (readSize != fileSize)
    {
        status = ERR(SCHED, TOCTOU);
        goto cleanup;
    }

    Elf64_File elf;
    if (elf64_validate(&elf, fileData, fileSize) != 0)
    {
        status = ERR(SCHED, INVALELF);
        goto cleanup;
    }

    Elf64_Addr minAddr = UINT64_MAX;
    Elf64_Addr maxAddr = 0;
    elf64_get_loadable_bounds(&elf, &minAddr, &maxAddr);
    uint64_t loadSize = maxAddr - minAddr;

    status = vmm_alloc(&process->space, (void**)&minAddr, loadSize, PAGE_SIZE, PML_USER | PML_WRITE | PML_PRESENT,
        VMM_ALLOC_OVERWRITE);
    if (IS_ERR(status))
    {
        goto cleanup;
    }

    elf64_load_segments(&elf, 0, 0);

    char* rsp = (char*)thread->userStack.top;

    addrs = malloc(sizeof(uintptr_t) * process->argc);
    if (addrs == NULL)
    {
        status = ERR(SCHED, NOMEM);
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
    ASM("cli");

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    thread->frame.rsp = ROUND_DOWN((uintptr_t)rsp - sizeof(uint64_t), 16);
    thread->frame.rip = elf.header->e_entry;
    thread->frame.rdi = process->argc;
    thread->frame.rsi = (uintptr_t)rsp;
    thread->frame.cs = GDT_CS_RING3;
    thread->frame.ss = GDT_SS_RING3;
    thread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    status = EOK;
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
    pid_t pid = process->id;
    if (status == OK)
    {
        thread_jump(thread);
    }
    LOG_DEBUG("exec failed due to %s pid=%llu\n", codetostr(ST_CODE(status)), pid);
    sched_exits("exec failed");
}

static void loader_entry(void)
{
    thread_t* thread = thread_current();

    WAIT_BLOCK(&thread->process->suspendQueue, !(atomic_load(&thread->process->flags) & PROCESS_SUSPENDED));

    file_table_close_mode(&thread->process->files, MODE_PRIVATE);

    loader_exec();
}

SYSCALL_DEFINE(SYS_SPAWN, const char** argv, spawn_flags_t flags)
{
    if (argv == NULL)
    {
        return ERR(SCHED, INVAL);
    }

    thread_t* thread = thread_current();
    assert(thread != NULL);
    process_t* process = thread->process;
    assert(process != NULL);

    namespace_t* ns = process_get_ns(process);
    UNREF_DEFER(ns);

    namespace_t* childNs;
    if (flags & SPAWN_EMPTY_NS || flags & SPAWN_COPY_NS)
    {
        childNs = namespace_new(ns);
        if (childNs == NULL)
        {
            return ERR(SCHED, NOMEM);
        }

        if (!(flags & SPAWN_EMPTY_NS))
        {
            status_t status = namespace_copy(childNs, ns);
            if (IS_ERR(status))
            {
                UNREF(childNs);
                return status;
            }
        }
    }
    else
    {
        childNs = REF(ns);
    }
    UNREF_DEFER(childNs);

    process_t* child;
    status_t status = process_new(&child, atomic_load(&process->priority),
        flags & SPAWN_EMPTY_GROUP ? NULL : &process->group, childNs);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(child);

    thread_t* childThread;
    status = thread_new(&childThread, child);
    if (IS_ERR(status))
    {
        return status;
    }

    char** argvCopy = NULL;
    uint64_t argc = 0;
    status = thread_copy_from_user_string_array(thread, argv, &argvCopy, &argc);
    if (IS_ERR(status))
    {
        return status;
    }

    if (argc == 0 || argvCopy[0] == NULL)
    {
        loader_strv_free(argvCopy, argc);
        return ERR(SCHED, INVAL);
    }

    status = process_set_cmdline(child, argvCopy, argc);
    if (IS_ERR(status))
    {
        loader_strv_free(argvCopy, argc);
        return status;
    }

    if (flags & SPAWN_SUSPEND)
    {
        atomic_fetch_or(&child->flags, PROCESS_SUSPENDED);
    }

    if (!(flags & SPAWN_EMPTY_FDS))
    {
        if (flags & SPAWN_STDIO_FDS)
        {
            file_table_copy(&child->files, &process->files, 0, 3);
        }
        else
        {
            file_table_copy(&child->files, &process->files, 0, CONFIG_MAX_FD);
        }
    }

    if (!(flags & SPAWN_EMPTY_ENV))
    {
        status = env_copy(&child->env, &process->env);
        if (IS_ERR(status))
        {
            loader_strv_free(argvCopy, argc);
            return status;
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

    *_result = child->id;
    sched_submit(childThread);
    return OK;
}

SYSCALL_DEFINE(SYS_THREAD_CREATE, void* entry, void* arg)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;
    space_t* space = &process->space;

    status_t status = space_check_access(space, entry, sizeof(uint64_t));
    if (IS_ERR(status))
    {
        return status;
    }

    // Dont check arg user space can use it however it wants

    thread_t* newThread;
    status = thread_new(&newThread, process);
    if (IS_ERR(status))
    {
        return status;
    }

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    newThread->frame.rip = (uint64_t)entry;
    newThread->frame.rsp = newThread->userStack.top;
    newThread->frame.rbp = newThread->userStack.top;
    newThread->frame.rdi = (uint64_t)arg;
    newThread->frame.cs = GDT_CS_RING3;
    newThread->frame.ss = GDT_SS_RING3;
    newThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    *_result = newThread->id;
    sched_submit(newThread);
    return OK;
}