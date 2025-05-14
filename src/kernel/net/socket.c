#include "socket.h"
#include "ctl.h"
#include "defs.h"
#include "log.h"
#include "pmm.h"
#include "process.h"
#include "sched.h"
#include "stdbool.h"
#include "sysfs.h"
#include "vfs.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/math.h>

static uint64_t socket_accept_read(file_t* file, void* buffer, uint64_t count)
{
    socket_t* socket = file->private;
    bool endOfSegment = false;
    uint64_t readCount = socket->family->receive(socket, buffer, count, &file->pos);
    return readCount;
}

static uint64_t socket_accept_write(file_t* file, const void* buffer, uint64_t count)
{
    socket_t* socket = file->private;
    return socket->family->send(socket, buffer, count);
}

static wait_queue_t* socket_accept_poll(file_t* file, poll_file_t* poll)
{
    socket_t* socket = file->private;
    return socket->family->poll(socket, poll);
}

static file_t* socket_accept_open(volume_t* volume, sysobj_t* sysobj)
{
    socket_t* socket = sysobj->dir->private;
    process_t* process = sched_process();
    if (process->id != socket->creator && !process_is_child(process, socket->creator))
    {
        return ERRPTR(EACCES);
    }

    socket_t* newSocket = malloc(sizeof(socket_t));
    if (socket == NULL)
    {
        return NULL;
    }
    newSocket->family = socket->family;
    newSocket->creator = socket->creator;

    if (socket->family->accept(socket, newSocket) == ERR)
    {
        free(socket);
        return NULL;
    }

    static file_ops_t fileOps = {
        .read = socket_accept_read,
        .write = socket_accept_write,
        .poll = socket_accept_poll,
    };
    file_t* file = file_new(volume);
    if (file == NULL)
    {
        free(socket);
        return NULL;
    }
    file->ops = &fileOps;
    file->private = newSocket;
    return file;
}

static void socket_accept_cleanup(sysobj_t* sysobj, file_t* file)
{
    socket_t* socket = file->private;
    socket->family->deinit(socket);
    free(socket);
}

static sysobj_ops_t acceptOps = {
    .open = socket_accept_open,
    .cleanup = socket_accept_cleanup,
};

static uint64_t socket_data_read(file_t* file, void* buffer, uint64_t count)
{
    socket_t* socket = file->sysobj->dir->private;
    uint64_t readCount = socket->family->receive(socket, buffer, count, &file->pos);
    return readCount;
}

static uint64_t socket_data_write(file_t* file, const void* buffer, uint64_t count)
{
    socket_t* socket = file->sysobj->dir->private;
    return socket->family->send(socket, buffer, count);
}

static wait_queue_t* socket_data_poll(file_t* file, poll_file_t* poll)
{
    socket_t* socket = file->sysobj->dir->private;
    return socket->family->poll(socket, poll);
}

static file_t* socket_data_open(volume_t* volume, sysobj_t* sysobj)
{
    socket_t* socket = sysobj->dir->private;
    process_t* process = sched_process();
    if (process->id != socket->creator && !process_is_child(process, socket->creator))
    {
        return ERRPTR(EACCES);
    }

    static file_ops_t fileOps = {
        .read = socket_data_read,
        .write = socket_data_write,
        .poll = socket_data_poll,
    };
    file_t* file = file_new(volume);
    if (file == NULL)
    {
        return NULL;
    }
    file->ops = &fileOps;
    return file;
}

static sysobj_ops_t dataOps = {
    .open = socket_data_open,
};

static uint64_t socket_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* socket = file->sysobj->dir->private;
    return socket->family->bind(socket, argv[1]);
}

static uint64_t socket_ctl_listen(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* socket = file->sysobj->dir->private;
    return socket->family->listen(socket);
}

static uint64_t socket_ctl_connect(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* socket = file->sysobj->dir->private;
    return socket->family->connect(socket, argv[1]);
}

CTL_STANDARD_WRITE_DEFINE(socket_ctl_write,
    (ctl_array_t){
        {"bind", socket_ctl_bind, 2, 2},
        {"listen", socket_ctl_listen, 1, 1},
        {"connect", socket_ctl_connect, 2, 2},
        {0},
    });

static file_t* socket_ctl_open(volume_t* volume, sysobj_t* sysobj)
{
    socket_t* socket = sysobj->dir->private;
    process_t* process = sched_process();
    if (process->id != socket->creator && !process_is_child(process, socket->creator))
    {
        return ERRPTR(EACCES);
    }

    static file_ops_t fileOps = {
        .write = socket_ctl_write,
    };
    file_t* file = file_new(volume);
    if (file == NULL)
    {
        return NULL;
    }
    file->ops = &fileOps;
    return file;
}

static sysobj_ops_t ctlOps = {
    .open = socket_ctl_open,
};

static void socket_on_free(sysdir_t* sysdir)
{
    socket_t* socket = sysdir->private;
    socket->family->deinit(socket);
    free(socket);
}

sysdir_t* socket_create(socket_family_t* family, const char* id)
{
    socket_t* socket = malloc(sizeof(socket_t));
    if (socket == NULL)
    {
        return NULL;
    }
    socket->family = family;
    socket->creator = sched_process()->id;

    if (family->init(socket) == ERR)
    {
        free(socket);
        return NULL;
    }

    char path[MAX_PATH];
    sprintf(path, "/net/%s", family->name);
    sysdir_t* socketDir = sysdir_new(path, id, socket_on_free, socket);
    if (socketDir == NULL)
    {
        family->deinit(socket);
        free(socket);
        return NULL;
    }

    if (sysdir_add(socketDir, "ctl", &ctlOps, NULL) == ERR || sysdir_add(socketDir, "data", &dataOps, NULL) == ERR ||
        sysdir_add(socketDir, "accept", &acceptOps, NULL) == ERR)
    {
        sysdir_free(socketDir);
        return NULL;
    }

    return socketDir;
}
