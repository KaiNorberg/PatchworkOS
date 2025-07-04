#include "socket.h"
#include "defs.h"
#include "fs/ctl.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "proc/process.h"
#include "sched/thread.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/math.h>

static atomic_uint64_t nextSocketId = ATOMIC_VAR_INIT(0);

static bool socket_has_access(socket_t* socket, process_t* process)
{
    return process->id == socket->creator || process_is_child(process, socket->creator);
}

static uint64_t socket_accept_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* socket = file->private;
    uint64_t readCount = socket->family->receive(socket, buffer, count, offset);
    return readCount;
}

static uint64_t socket_accept_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* socket = file->private;
    uint64_t result = socket->family->send(socket, buffer, count, offset);
    return result;
}

static wait_queue_t* socket_accept_poll(file_t* file, poll_file_t* poll)
{
    socket_t* socket = file->private;
    return socket->family->poll(socket, poll);
}

static uint64_t socket_accept_open(file_t* file)
{
    socket_t* socket = file->dentry->inode->private;
    if (!socket_has_access(socket, sched_process()))
    {
        errno = EACCES;
        return ERR;
    }

    socket_t* newSocket = heap_alloc(sizeof(socket_t), HEAP_NONE);
    if (newSocket == NULL)
    {
        return ERR;
    }
    newSocket->family = socket->family;
    newSocket->creator = socket->creator;
    newSocket->flags = socket->flags;
    newSocket->private = NULL;

    if (socket->family->accept(socket, newSocket) == ERR)
    {
        heap_free(newSocket);
        return ERR;
    }

    file->private = newSocket;
    return 0;
}

static void socket_accept_cleanup(file_t* file)
{
    socket_t* socket = file->private;
    if (socket != NULL)
    {
        socket->family->deinit(socket);
        heap_free(socket);
    }
}

static file_ops_t acceptOps = {
    .open = socket_accept_open,
    .read = socket_accept_read,
    .write = socket_accept_write,
    .poll = socket_accept_poll,
    .cleanup = socket_accept_cleanup,
};

static uint64_t socket_data_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* socket = file->private;
    return socket->family->receive(socket, buffer, count, offset);
}

static uint64_t socket_data_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* socket = file->private;
    return socket->family->send(socket, buffer, count, offset);
}

static wait_queue_t* socket_data_poll(file_t* file, poll_file_t* poll)
{
    socket_t* socket = file->private;
    return socket->family->poll(socket, poll);
}

static uint64_t socket_data_open(file_t* file)
{
    socket_t* socket = file->dentry->inode->private;
    if (!socket_has_access(socket, sched_process()))
    {
        errno = EACCES;
        return ERR;
    }

    file->private = socket;
    return 0;
}

static file_ops_t dataOps = {
    .open = socket_data_open,
    .read = socket_data_read,
    .write = socket_data_write,
    .poll = socket_data_poll,
};

static uint64_t socket_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* socket = file->private;
    return socket->family->bind(socket, argv[1]);
}

static uint64_t socket_ctl_listen(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* socket = file->private;
    return socket->family->listen(socket);
}

static uint64_t socket_ctl_connect(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* socket = file->private;
    return socket->family->connect(socket, argv[1]);
}

CTL_STANDARD_WRITE_DEFINE(socket_ctl_write,
    (ctl_array_t){
        {"bind", socket_ctl_bind, 2, 2},
        {"listen", socket_ctl_listen, 1, 1},
        {"connect", socket_ctl_connect, 2, 2},
        {0},
    });

static uint64_t socket_ctl_open(file_t* file)
{
    socket_t* socket = file->dentry->inode->private;
    if (!socket_has_access(socket, sched_process()))
    {
        errno = EACCES;
        return ERR;
    }

    file->private = socket;
    return 0;
}

static file_ops_t ctlOps = {
    .open = socket_ctl_open,
    .write = socket_ctl_write,
};

socket_t* socket_new(socket_family_t* family, path_flags_t flags)
{
    socket_t* socket = heap_alloc(sizeof(socket_t), HEAP_NONE);
    if (socket == NULL)
    {
        return NULL;
    }

    uint64_t id = atomic_fetch_add(&nextSocketId, 1);
    snprintf(socket->id, sizeof(socket->id), "%llu", id);

    socket->family = family;
    socket->creator = sched_process()->id;
    socket->flags = flags;
    socket->private = NULL;

    if (family->init(socket) == ERR)
    {
        heap_free(socket);
        return NULL;
    }

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "/net/%s", family->name);

    if (sysfs_dir_init(&socket->dir, path, socket->id, socket) == ERR)
    {
        family->deinit(socket);
        heap_free(socket);
        return NULL;
    }

    if (sysfs_file_init(&socket->ctlFile, &socket->dir, "ctl", &ctlOps, socket) == ERR ||
        sysfs_file_init(&socket->dataFile, &socket->dir, "data", &dataOps, socket) == ERR ||
        sysfs_file_init(&socket->acceptFile, &socket->dir, "accept", &acceptOps, socket) == ERR)
    {
        sysfs_dir_deinit(&socket->dir, NULL);
        family->deinit(socket);
        heap_free(socket);
    }

    return socket;
}

static void socket_on_free(sysfs_dir_t* sysfs_dir)
{
    socket_t* socket = sysfs_dir->private;
    if (socket != NULL)
    {
        socket->family->deinit(socket);
        heap_free(socket);
    }
}

void socket_free(socket_t* socket)
{
    sysfs_dir_deinit(&socket->dir, socket_on_free);
}