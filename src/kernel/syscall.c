#include "syscall.h"

#include <errno.h>
#include <string.h>

#include "defs.h"
#include "process.h"
#include "sched.h"
#include "smp.h"
#include "time.h"
#include "vfs.h"
#include "vmm.h"

// NOTE: Syscalls should always return a 64 bit value to prevent garbage from remaining in rax.

// TODO: Improve verify funcs, improve multithreading string safety. copy_to_user? copy_from_user?
static bool verify_pointer(const void* pointer, uint64_t length)
{
    if (pointer == NULL)
    {
        return false;
    }

    if ((uint64_t)pointer + length > VMM_LOWER_HALF_MAX)
    {
        return false;
    }

    return true;
}

static bool verify_buffer(const void* pointer, uint64_t length)
{
    if (!verify_pointer(pointer, length))
    {
        return false;
    }

    if (!vmm_mapped(pointer, length))
    {
        return false;
    }

    return true;
}

static bool verify_string(const char* string)
{
    return verify_buffer(string, 0) && verify_buffer(string, strlen(string));
}

///////////////////////////////////////////////////////

NORETURN void syscall_process_exit(uint64_t status)
{
    sched_process_exit(status);
}

NORETURN void syscall_thread_exit(void)
{
    sched_thread_exit();
}

pid_t syscall_spawn(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return sched_spawn(path, THREAD_PRIORITY_MIN);
}

uint64_t syscall_sleep(nsec_t nanoseconds)
{
    return sched_sleep(NULL, nanoseconds);
}

errno_t syscall_error(void)
{
    return sched_thread()->error;
}

pid_t syscall_pid(void)
{
    return sched_process()->id;
}

tid_t syscall_tid(void)
{
    return sched_thread()->id;
}

nsec_t syscall_uptime(void)
{
    return time_uptime();
}

fd_t syscall_open(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_open(path);
    if (file == NULL)
    {
        return ERR;
    }

    fd_t fd = vfs_context_open(file);
    if (fd == ERR)
    {
        file_deref(file);
        return ERR;
    }

    return fd;
}

uint64_t syscall_close(fd_t fd)
{
    return vfs_context_close(fd);
}

uint64_t syscall_read(fd_t fd, void* buffer, uint64_t count)
{
    if (!verify_buffer(buffer, count))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return vfs_read(file, buffer, count);
}

uint64_t syscall_write(fd_t fd, const void* buffer, uint64_t count)
{
    if (!verify_buffer(buffer, count))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return vfs_write(file, buffer, count);
}

uint64_t syscall_seek(fd_t fd, int64_t offset, uint8_t origin)
{
    file_t* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return vfs_seek(file, offset, origin);
}

uint64_t syscall_ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size)
{
    if (!verify_buffer(argp, size))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return vfs_ioctl(file, request, argp, size);
}

uint64_t syscall_realpath(char* out, const char* path)
{
    if (!verify_pointer(out, 0) || !verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return vfs_realpath(out, path);
}

uint64_t syscall_chdir(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return vfs_chdir(path);
}

uint64_t syscall_poll(pollfd_t* fds, uint64_t amount, uint64_t timeout)
{
    if (amount == 0 || amount > CONFIG_MAX_FILE)
    {
        return ERROR(EINVAL);
    }

    if (!verify_buffer(fds, sizeof(pollfd_t) * amount))
    {
        return ERROR(EFAULT);
    }

    poll_file_t files[CONFIG_MAX_FILE];
    for (uint64_t i = 0; i < amount; i++)
    {
        files[i].file = vfs_context_get(fds[i].fd);
        if (files[i].file == NULL)
        {
            return ERR;
        }

        files[i].requested = fds[i].requested;
        files[i].occurred = 0;
    }

    uint64_t result = vfs_poll(files, amount, timeout);

    for (uint64_t i = 0; i < amount; i++)
    {
        fds[i].occurred = files[i].occurred;
        file_deref(files[i].file);
    }

    return result;
}

uint64_t syscall_stat(const char* path, stat_t* buffer)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    if (!verify_buffer(buffer, sizeof(stat_t)))
    {
        return ERROR(EFAULT);
    }

    return vfs_stat(path, buffer);
}

void* syscall_mmap(fd_t fd, void* address, uint64_t length, prot_t prot)
{
    file_t* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return NULL;
    }
    FILE_GUARD(file);

    return vfs_mmap(file, address, length, prot);
}

uint64_t syscall_munmap(void* address, uint64_t length)
{
    if (!verify_pointer(address, length))
    {
        return ERROR(EFAULT);
    }

    return vmm_unmap(address, length);
}

uint64_t syscall_mprotect(void* address, uint64_t length, prot_t prot)
{
    if (!verify_pointer(address, length))
    {
        return ERROR(EFAULT);
    }

    return vmm_protect(address, length, prot);
}

uint64_t syscall_flush(fd_t fd, const void* buffer, uint64_t size, const rect_t* rect)
{
    if (!verify_buffer(buffer, size))
    {
        return ERROR(EFAULT);
    }

    if (!verify_buffer(rect, sizeof(rect_t)))
    {
        return ERROR(EFAULT);
    }

    file_t* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return vfs_flush(file, buffer, size, rect);
}

///////////////////////////////////////////////////////

void syscall_handler_end(void)
{
    if (sched_process()->killed)
    {
        sched_thread_exit();
    }

    SMP_SEND_IPI_TO_SELF(IPI_SCHEDULE);
}

void* syscallTable[] = {
    syscall_process_exit,
    syscall_thread_exit,
    syscall_spawn,
    syscall_sleep,
    syscall_error,
    syscall_pid,
    syscall_tid,
    syscall_uptime,
    syscall_open,
    syscall_close,
    syscall_read,
    syscall_write,
    syscall_seek,
    syscall_ioctl,
    syscall_realpath,
    syscall_chdir,
    syscall_poll,
    syscall_stat,
    syscall_mmap,
    syscall_munmap,
    syscall_mprotect,
    syscall_flush,
};
