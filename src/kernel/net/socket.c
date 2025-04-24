#include "socket.h"
#include "defs.h"
#include "pmm.h"
#include "process.h"
#include "stdbool.h"
#include "sysfs.h"
#include "vfs.h"
#include "sched.h"
#include "actions.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/math.h>

static file_ops_t dataOps =
{

};

SYSFS_STANDARD_RESOURCE_OPS_DEFINE(dataResOps, &dataOps);

static uint64_t socket_action_bind(uint64_t argc, const char** argv, void* private)
{
    socket_t* socket = private;
    if (socket->state != SOCKET_BLANK || socket->family->bind == NULL)
    {
        return ERROR(ENOOP);
    }
    return socket->family->bind(socket, argv[1]);
}

static actions_t actions =
{
    {"bind", socket_action_bind, 2, 2},
    {0}
};

static uint64_t socket_ctl_write(file_t* file, const void* buffer, uint64_t count)
{
    socket_t* socket = file->resource->dir->private;
    process_t* process = sched_process();
    if (process->id != socket->creator && !process_is_child(process, socket->creator))
    {
        return ERROR(EACCES);
    }

    return actions_dispatch(&actions, buffer, count, socket);
}

static file_ops_t ctlOps =
{
    .write = socket_ctl_write,
};

SYSFS_STANDARD_RESOURCE_OPS_DEFINE(ctlResOps, &ctlOps);

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
    socket->state = SOCKET_BLANK;
    socket->creator = sched_process()->id;
    lock_init(&socket->lock);

    if (family->init(socket) == ERR)
    {
        free(socket);
        return NULL;
    }

    char path[MAX_PATH];
    sprintf(path, "%s/%s", "/net", family->name);
    sysdir_t* socketDir = sysdir_new(path, id, socket_on_free, socket);
    if (socketDir == NULL)
    {
        family->deinit(socket);
        free(socket);
        return NULL;
    }

    if (sysdir_add(socketDir, "ctl", &ctlResOps, NULL) == ERR || sysdir_add(socketDir, "data", &dataResOps, NULL) == ERR)
    {
        sysdir_free(socketDir);
        return NULL;
    }

    return socketDir;
}
