#include <kernel/cpu/gdt.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/file_table.h>
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

void loader_exec(void)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    file_t* file = NULL;
    void* fileData = NULL;

    uintptr_t* addrs = NULL;

    status_t status = vfs_open(&file, NULL, process->args, process);
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
    vfs_seek(file, 0, IOSEEK_END, &fileSize);
    vfs_seek(file, 0, IOSEEK_START, NULL);

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

    uint64_t argc = 0;
    for (size_t i = 0; i < process->argsLen; i++)
    {
        if (process->args[i] == '\0')
        {
            argc++;
        }
    }

    addrs = malloc(sizeof(uintptr_t) * argc);
    if (addrs == NULL)
    {
        status = ERR(SCHED, NOMEM);
        goto cleanup;
    }

    rsp -= process->argsLen;
    memcpy(rsp, process->args, process->argsLen);

    size_t offset = 0;
    for (uint64_t i = 0; i < argc; i++)
    {
        addrs[i] = (uintptr_t)rsp + offset;
        offset += strlen(process->args + offset) + 1;
    }

    rsp = (char*)ROUND_DOWN((uintptr_t)rsp, 8);

    rsp -= (argc + 1) * sizeof(char*);
    uintptr_t* argvStack = (uintptr_t*)rsp;
    for (uint64_t i = 0; i < argc; i++)
    {
        argvStack[i] = addrs[i];
    }
    argvStack[argc] = 0;

    // Disable interrupts, they will be enabled when we jump to user space.
    ASM("cli");

    memset(&thread->frame, 0, sizeof(interrupt_frame_t));
    thread->frame.rsp = ROUND_DOWN((uintptr_t)rsp - sizeof(uint64_t), 16);
    thread->frame.rip = elf.header->e_entry;
    thread->frame.rdi = argc;
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
    proc_t pid = process->id;
    if (status == OK)
    {
        thread_jump(thread);
    }
    LOG_DEBUG("exec failed due to %Y pid=%llu\n", status, pid);
    sched_exit("exec failed");
}

static void loader_entry(void)
{
    thread_t* thread = thread_current();

    WAIT_BLOCK(&thread->process->suspendQueue, !(atomic_load(&thread->process->flags) & PROCESS_SUSPENDED));

    file_table_drop_mode(&thread->process->files, MODE_PRIVATE);

    loader_exec();
}

SYSCALL_DEFINE(SYS_PROC_CREATE, fd_t* proc, const char* args, size_t argsLen, const proc_fd_t* fds, size_t count, prio_t priority, proc_flags_t flags)
{
    if (args == NULL || argsLen == 0)
    {
        return ERR(SCHED, INVAL);
    }

    thread_t* thread = thread_current();
    assert(thread != NULL);
    process_t* process = thread->process;
    assert(process != NULL);

    process_t* child;
    status_t status = process_new(&child, priority, (flags & PROC_GROUP) ? &process->group : NULL);
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

    char* argsCopy = malloc(argsLen);
    if (argsCopy == NULL)
    {
        return ERR(SCHED, NOMEM);
    }

    status = space_copy_in(&process->space, argsCopy, args, argsLen);
    if (IS_ERR(status))
    {
        free(argsCopy);
        return status;
    }

    status = process_set_cmdline(child, argsCopy, argsLen);
    free(argsCopy);
    if (IS_ERR(status))
    {
        return status;
    }

    if (flags & PROC_SUSPEND)
    {
        atomic_fetch_or(&child->flags, PROCESS_SUSPENDED);
    }

    if (count > 0 && fds != NULL)
    {
        proc_fd_t* fdsCopy = malloc(sizeof(proc_fd_t) * count);
        if (fdsCopy == NULL)
        {
            return ERR(SCHED, NOMEM);
        }

        status = space_copy_in(&process->space, fdsCopy, fds, sizeof(proc_fd_t) * count);
        if (IS_ERR(status))
        {
            free(fdsCopy);
            return status;
        }

        for (size_t i = 0; i < count; i++)
        {
            file_t* file = file_table_get(&process->files, fdsCopy[i].parent);
            if (file != NULL)
            {
                file_table_set(&child->files, fdsCopy[i].child, file);
                UNREF(file);
            }
        }
        free(fdsCopy);
    }

    if (!(flags & PROC_DETACHED))
    {
        /// @todo Reimplement procfs.
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

SYSCALL_DEFINE(SYS_THRD_CREATE, void* entry, void* arg)
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