#include "syscall.h"

#include "config.h"
#include "cpu/gdt.h"
#include "cpu/regs.h"
#include "cpu/smp.h"
#include "cpu/vectors.h"
#include "defs.h"
#include "drivers/systime/systime.h"
#include "fs/vfs.h"
#include "fs/vfs_ctx.h"
#include "gdt.h"
#include "ipc/pipe.h"
#include "kernel.h"
#include "mem/vmm.h"
#include "sched/loader.h"
#include "sched/sched.h"
#include "sched/thread.h"
#include "utils/log.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// NOTE: Syscalls should always return a 64 bit value to prevent garbage from remaining in rax.

// TODO: Improve verify funcs, improve multithreading string safety. copy_to_user? copy_from_user?
static bool pointer_is_valid(const void* pointer, uint64_t length)
{
    if (length == 0)
    {
        return true;
    }

    if (pointer == NULL)
    {
        return false;
    }

    uintptr_t start = (uintptr_t)pointer;
    uintptr_t end = (uintptr_t)pointer + length;

    if (start > end) // Overflow
    {
        return false;
    }

    if (end > VMM_LOWER_HALF_END || (uintptr_t)start < VMM_LOWER_HALF_START)
    {
        return false;
    }

    return true;
}

static bool buffer_is_valid(space_t* space, const void* pointer, uint64_t length)
{
    if (length == 0)
    {
        return true;
    }

    if (!pointer_is_valid(pointer, length))
    {
        return false;
    }

    if (!vmm_mapped(space, pointer, length))
    {
        return false;
    }

    return true;
}

static bool string_is_valid(space_t* space, const char* string)
{
    if (!buffer_is_valid(space, string, sizeof(const char*)))
    {
        return false;
    }

    const char* chr = string;
    while (true)
    {
        if (!buffer_is_valid(space, chr, sizeof(char)))
        {
            return false;
        }

        if (*chr == '\0')
        {
            return true;
        }

        chr++;
    }
}

///////////////////////////////////////////////////////

void syscall_process_exit(uint64_t status)
{
    sched_process_exit(status);
    sched_invoke();
    assert(false);
}

void syscall_thread_exit(void)
{
    sched_thread_exit();
    sched_invoke();
    assert(false);
}

pid_t syscall_spawn(const char** argv, const spawn_fd_t* fds, const char* cwd, spawn_attr_t* attr)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;
    space_t* space = &process->space;

    if (cwd != NULL && !string_is_valid(space, cwd))
    {
        return ERROR(EFAULT);
    }

    if (attr != NULL && !buffer_is_valid(space, attr, sizeof(spawn_attr_t)))
    {
        return ERROR(EFAULT);
    }

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
        return ERROR(EACCES);
    }

    uint64_t argc = 0;
    while (1)
    {
        if (!buffer_is_valid(space, &argv[argc], sizeof(const char*)))
        {
            return ERROR(EFAULT);
        }
        else if (argv[argc] == NULL)
        {
            break;
        }
        else if (!string_is_valid(space, argv[argc]))
        {
            return ERROR(EFAULT);
        }

        argc++;
    }

    uint64_t fdAmount = 0;
    if (fds != NULL)
    {
        while (1)
        {
            if (fdAmount >= CONFIG_MAX_FD)
            {
                return ERROR(EINVAL);
            }
            else if (!buffer_is_valid(space, &fds[fdAmount], sizeof(fd_t)))
            {
                return ERROR(EFAULT);
            }
            else if (fds[fdAmount].child == FD_NONE || fds[fdAmount].parent == FD_NONE)
            {
                break;
            }

            fdAmount++;
        }
    }

    thread_t* child = loader_spawn(argv, priority, cwd == NULL ? NULL : PATH(process, cwd));
    if (child == NULL)
    {
        return ERR;
    }

    vfs_ctx_t* parentVfsCtx = &process->vfsCtx;
    vfs_ctx_t* childVfsCtx = &child->process->vfsCtx;

    for (uint64_t i = 0; i < fdAmount; i++)
    {
        file_t* file = vfs_ctx_file(parentVfsCtx, fds[i].parent);
        if (file == NULL)
        {
            thread_free(child);
            return ERROR(EBADF);
        }
        FILE_DEFER(file);

        if (vfs_ctx_openas(childVfsCtx, fds[i].child, file) == ERR)
        {
            thread_free(child);
            return ERROR(EBADF);
        }
    }

    sched_push(child, thread, NULL);
    return child->process->id;
}

uint64_t syscall_sleep(clock_t nanoseconds)
{
    return sched_sleep(nanoseconds);
}

errno_t syscall_last_error(void)
{
    return sched_thread()->error;
}

pid_t syscall_getpid(void)
{
    return sched_process()->id;
}

tid_t syscall_gettid(void)
{
    return sched_thread()->id;
}

clock_t syscall_uptime(void)
{
    return systime_uptime();
}

time_t syscall_unix_epoch(time_t* timePtr)
{
    time_t epoch = systime_unix_epoch();
    if (timePtr != NULL)
    {
        if (!pointer_is_valid(timePtr, sizeof(time_t)))
        {
            return ERROR(EFAULT);
        }

        *timePtr = epoch;
    }

    return epoch;
}

fd_t syscall_open(const char* path)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!string_is_valid(space, path))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_open(PATH(process, path));
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_ctx_open(&process->vfsCtx, file);
}

uint64_t syscall_open2(const char* path, fd_t fds[2])
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!string_is_valid(space, path))
    {
        return ERROR(EFAULT);
    }

    if (!buffer_is_valid(space, fds, sizeof(fd_t) * 2))
    {
        return ERROR(EFAULT);
    }

    file_t* files[2];
    if (vfs_open2(PATH(process, path), files) == ERR)
    {
        return ERR;
    }
    FILE_DEFER(files[0]);
    FILE_DEFER(files[1]);

    fds[0] = vfs_ctx_open(&process->vfsCtx, files[0]);
    if (fds[0] == ERR)
    {
        return ERR;
    }
    fds[1] = vfs_ctx_open(&process->vfsCtx, files[1]);
    if (fds[1] == ERR)
    {
        vfs_ctx_close(&process->vfsCtx, fds[0]);
        return ERR;
    }

    return 0;
}

uint64_t syscall_close(fd_t fd)
{
    process_t* process = sched_process();

    return vfs_ctx_close(&process->vfsCtx, fd);
}

uint64_t syscall_read(fd_t fd, void* buffer, uint64_t count)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!buffer_is_valid(space, buffer, count))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_ctx_file(&process->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_read(file, buffer, count);
}

uint64_t syscall_write(fd_t fd, const void* buffer, uint64_t count)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!buffer_is_valid(space, buffer, count))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_ctx_file(&process->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_write(file, buffer, count);
}

uint64_t syscall_seek(fd_t fd, int64_t offset, seek_origin_t origin)
{
    process_t* process = sched_process();

    file_t* file = vfs_ctx_file(&process->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_seek(file, offset, origin);
}

uint64_t syscall_ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (argp != NULL && !buffer_is_valid(space, argp, size))
    {
        return ERROR(EFAULT);
    }

    if (argp == NULL && size != 0)
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_ctx_file(&process->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_ioctl(file, request, argp, size);
}

uint64_t syscall_chdir(const char* path)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!string_is_valid(space, path))
    {
        return ERROR(EFAULT);
    }

    return vfs_chdir(PATH(process, path));
}

uint64_t syscall_poll(pollfd_t* fds, uint64_t amount, clock_t timeout)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (amount == 0 || amount >= CONFIG_MAX_FD)
    {
        return ERROR(EINVAL);
    }

    if (!buffer_is_valid(space, fds, sizeof(pollfd_t) * amount))
    {
        return ERROR(EFAULT);
    }

    poll_file_t files[CONFIG_MAX_FD];
    for (uint64_t i = 0; i < amount; i++)
    {
        files[i].file = vfs_ctx_file(&process->vfsCtx, fds[i].fd);
        if (files[i].file == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                file_deref(files[j].file);
            }
            return ERR;
        }

        files[i].events = fds[i].events;
        files[i].revents = 0;
    }

    uint64_t result = vfs_poll(files, amount, timeout);

    for (uint64_t i = 0; i < amount; i++)
    {
        fds[i].revents = files[i].revents;
        file_deref(files[i].file);
    }

    return result;
}

uint64_t syscall_stat(const char* path, stat_t* buffer)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!string_is_valid(space, path))
    {
        return ERROR(EFAULT);
    }

    if (!buffer_is_valid(space, buffer, sizeof(stat_t)))
    {
        return ERROR(EFAULT);
    }

    return vfs_stat(PATH(process, path), buffer);
}

void* syscall_mmap(fd_t fd, void* address, uint64_t length, prot_t prot)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    file_t* file = vfs_ctx_file(&process->vfsCtx, fd);
    if (file == NULL)
    {
        return NULL;
    }
    FILE_DEFER(file);

    return vfs_mmap(file, address, length, prot);
}

uint64_t syscall_munmap(void* address, uint64_t length)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!pointer_is_valid(address, length))
    {
        return ERROR(EFAULT);
    }

    return vmm_unmap(space, address, length);
}

uint64_t syscall_mprotect(void* address, uint64_t length, prot_t prot)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!pointer_is_valid(address, length))
    {
        return ERROR(EFAULT);
    }

    return vmm_protect(space, address, length, prot);
}

uint64_t syscall_readdir(fd_t fd, stat_t* infos, uint64_t amount)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!buffer_is_valid(space, infos, amount * sizeof(stat_t)))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_ctx_file(&process->vfsCtx, fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_DEFER(file);

    return vfs_readdir(file, infos, amount);
}

tid_t syscall_thread_create(void* entry, void* arg)
{
    thread_t* thread = sched_thread();
    process_t* process = thread->process;
    space_t* space = &process->space;

    if (!buffer_is_valid(space, entry, sizeof(uint64_t)))
    {
        return ERROR(EFAULT);
    }

    thread_t* newThread = loader_thread_create(process, entry, arg);
    if (newThread == NULL)
    {
        return ERR;
    }

    sched_push(newThread, thread, NULL);
    return newThread->id;
}

uint64_t syscall_yield(void)
{
    sched_yield();
    sched_invoke();
    return 0;
}

fd_t syscall_dup(fd_t oldFd)
{
    return vfs_ctx_dup(&sched_process()->vfsCtx, oldFd);
}

fd_t syscall_dup2(fd_t oldFd, fd_t newFd)
{
    return vfs_ctx_dup2(&sched_process()->vfsCtx, oldFd, newFd);
}

uint64_t syscall_futex(atomic_uint64_t* addr, uint64_t val, futex_op_t op, clock_t timeout)
{
    return futex_do(addr, val, op, timeout);
}

uint64_t syscall_rename(const char* oldpath, const char* newpath)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!string_is_valid(space, oldpath) || !string_is_valid(space, newpath))
    {
        return ERROR(EFAULT);
    }

    return vfs_rename(PATH(process, oldpath), PATH(process, newpath));
}

uint64_t syscall_remove(const char* path)
{
    process_t* process = sched_process();
    space_t* space = &process->space;

    if (!string_is_valid(space, path))
    {
        return ERROR(EFAULT);
    }

    return vfs_remove(PATH(process, path));
}

///////////////////////////////////////////////////////

static void* syscallTable[] = {
    syscall_process_exit,
    syscall_thread_exit,
    syscall_spawn,
    syscall_sleep,
    syscall_last_error,
    syscall_getpid,
    syscall_gettid,
    syscall_uptime,
    syscall_unix_epoch,
    syscall_open,
    syscall_open2,
    syscall_close,
    syscall_read,
    syscall_write,
    syscall_seek,
    syscall_ioctl,
    syscall_chdir,
    syscall_poll,
    syscall_stat,
    syscall_mmap,
    syscall_munmap,
    syscall_mprotect,
    syscall_readdir,
    syscall_thread_create,
    syscall_yield,
    syscall_dup,
    syscall_dup2,
    syscall_futex,
    syscall_rename,
    syscall_remove,
};

void syscall_init(void)
{
    // Read this to understand whats happening https://www.felixcloutier.com/x86/syscall,
    // https://www.felixcloutier.com/x86/sysret.

    msr_write(MSR_EFER, msr_read(MSR_EFER) | EFER_SYSCALL_ENABLE);

    msr_write(MSR_STAR, ((uint64_t)(GDT_USER_CODE - 16) | GDT_RING3) << 48 | ((uint64_t)(GDT_KERNEL_CODE)) << 32);
    msr_write(MSR_LSTAR, (uint64_t)syscall_entry);

    msr_write(MSR_SYSCALL_FLAG_MASK,
        RFLAGS_TRAP | RFLAGS_DIRECTION | RFLAGS_INTERRUPT_ENABLE | RFLAGS_IOPL | RFLAGS_AUX_CARRY | RFLAGS_NESTED_TASK);
}

void syscall_ctx_init(syscall_ctx_t* ctx, uint64_t kernelRsp)
{
    ctx->kernelRsp = kernelRsp;
    ctx->userRsp = 0;
    ctx->inSyscall = false;
}

void syscall_ctx_load(syscall_ctx_t* ctx)
{
    // We use the gs register to keep track of the kernel stack to switch to and to save the user stack.
    msr_write(MSR_GS_BASE, (uint64_t)ctx);
    msr_write(MSR_KERNEL_GS_BASE, (uint64_t)ctx);
}

void syscall_handler(trap_frame_t* trapFrame)
{
    uint64_t selector = trapFrame->rax;
    if (selector >= sizeof(syscallTable) / sizeof(syscallTable[0]))
    {
        trapFrame->rax = ERR;
        return;
    }

    thread_t* thread = sched_thread();
    thread->syscall.inSyscall = true;

    uint64_t (*syscall)(uint64_t a, uint64_t b, uint64_t c, uint64_t d, uint64_t e, uint64_t f) =
        syscallTable[selector];
    trapFrame->rax =
        syscall(trapFrame->rdi, trapFrame->rsi, trapFrame->rdx, trapFrame->r10, trapFrame->r8, trapFrame->r9);

    thread->syscall.inSyscall = false;

    // No need to invoke scheduler due to tickless system.
}
