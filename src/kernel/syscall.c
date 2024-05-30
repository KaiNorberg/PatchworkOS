#include "syscall.h"

#include <string.h>
#include <errno.h>

#include "defs.h"
#include "tty.h"
#include "heap.h"
#include "vmm.h"
#include "time.h"
#include "utils.h"
#include "smp.h"
#include "debug.h"
#include "sched.h"
#include "loader.h"
#include "net.h"

//NOTE: Syscalls should always return a 64 bit value to prevent garbage from remaining in rax.

//TODO: Improve verify funcs, improve multithreading string safety.
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

    /*if (!vmm_mapped(pointer, length))
    {
        return false;
    }*/

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

uint64_t syscall_spawn(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    return sched_spawn(path);
}

uint64_t syscall_sleep(uint64_t nanoseconds)
{
    sched_sleep(nanoseconds);
    return 0;
}

uint64_t syscall_error(void)
{
    return sched_thread()->error;
}

uint64_t syscall_pid(void)
{
    return sched_process()->id;
}

uint64_t syscall_tid(void)
{
    return sched_thread()->id;
}

uint64_t syscall_uptime(void)
{
    return time_nanoseconds();
}

uint64_t syscall_open(const char* path)
{
    if (!verify_string(path))
    {
        return ERROR(EFAULT);
    }

    File* file = vfs_open(path);
    if (file == NULL)
    {
        return ERR;
    }

    fd_t fd = vfs_context_open(file);
    if (fd == ERR)
    {
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

    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return FILE_CALL_METHOD(file, read, buffer, count);
}

uint64_t syscall_write(fd_t fd, const void* buffer, uint64_t count)
{
    if (!verify_buffer(buffer, count))
    {
        return ERROR(EFAULT);
    }

    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return FILE_CALL_METHOD(file, write, buffer, count);
}

uint64_t syscall_seek(fd_t fd, int64_t offset, uint8_t origin)
{
    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return FILE_CALL_METHOD(file, seek, offset, origin);
}

uint64_t syscall_ioctl(fd_t fd, uint64_t request, void* buffer, uint64_t length)
{
    if (!verify_buffer(buffer, length))
    {
        return ERROR(EFAULT);
    }

    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    return FILE_CALL_METHOD(file, ioctl, request, buffer, length);
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

    PollFile files[CONFIG_MAX_FILE + 1];
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
    files[amount].file = NULL;

    uint64_t result = vfs_poll(files, timeout);

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

void* syscall_mmap(fd_t fd, void* address, uint64_t length, uint8_t prot)
{
    File* file = vfs_context_get(fd);
    if (file == NULL)
    {
        return NULL;
    }
    FILE_GUARD(file);

    return FILE_CALL_METHOD_PTR(file, mmap, address, length, prot);
}

uint64_t syscall_munmap(void* address, uint64_t length)
{
    if (!verify_pointer(address, length))
    {
        return ERROR(EFAULT);
    }

    return vmm_unmap(address, length);
}

uint64_t syscall_mprotect(void* address, uint64_t length, uint8_t prot)
{
    if (!verify_pointer(address, length))
    {
        return ERROR(EFAULT);
    }
    
    return vmm_protect(address, length, prot);
}

fd_t syscall_announce(const char* address)
{
    if (!verify_string(address))
    {
        return ERROR(EFAULT);
    }

    File* file = net_announce(address);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);

    fd_t fd = vfs_context_open(file);
    if (fd == ERR)
    {
        return ERR;
    }

    return fd;
}

fd_t syscall_dial(const char* address)
{
    if (!verify_string(address))
    {
        return ERROR(EFAULT);
    }

    File* file = net_dial(address);
    if (file == NULL)
    {
        return ERR;
    }
    FILE_GUARD(file);
    
    fd_t fd = vfs_context_open(file);
    if (fd == ERR)
    {
        return ERR;
    }

    return fd;
}

fd_t syscall_accept(fd_t fd)
{
    File* server = vfs_context_get(fd);
    if (server == NULL)
    {
        return ERR;
    }
    FILE_GUARD(server);

    File* client = FILE_CALL_METHOD_PTR(server, accept);
    if (client == NULL)
    {
        return ERR;
    }

    fd_t clientFd = vfs_context_open(client);
    if (clientFd == ERR)
    {
        return ERR;
    }

    return clientFd;
}

///////////////////////////////////////////////////////

void syscall_handler_end(void)
{
    if (sched_process()->killed)
    {
        sched_thread_exit();
    }

    sched_yield();
}

void* syscallTable[] =
{
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
    syscall_announce,
    syscall_dial,
    syscall_accept
};