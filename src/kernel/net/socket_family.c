#include "socket_family.h"

#include "defs.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "sched/thread.h"
#include "socket.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/math.h>

static uint64_t socket_family_new_read(file_t* file, void* buffer, uint64_t count)
{
    socket_t* socket = file->private;

    uint64_t len = strlen(socket->id);
    return BUFFER_READ(file, buffer, count, socket->id, len + 1); // Include null terminator
}

static uint64_t socket_family_new_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    socket_t* socket = file->private;

    uint64_t len = strlen(socket->id);
    return BUFFER_SEEK(file, offset, origin, len + 1); // Include null terminator
}

static file_t* socket_family_new_open(volume_t* volume, const path_t* path, sysobj_t* sysobj)
{
    socket_family_t* family = sysobj->dir->private;

    socket_t* socket = socket_new(family, path->flags);
    if (socket == NULL)
    {
        return NULL;
    }

    file_t* file = file_new(volume, path, PATH_NONBLOCK);
    if (file == NULL)
    {
        socket_free(socket);
        return NULL;
    }
    static file_ops_t fileOps = {
        .read = socket_family_new_read,
        .seek = socket_family_new_seek,
    };
    file->ops = &fileOps;
    file->private = socket;
    return file;
}

static void socket_family_new_cleanup(sysobj_t* sysobj, file_t* file)
{
    socket_t* socket = file->private;
    if (socket != NULL)
    {
        socket_free(socket);
    }
}

static sysobj_ops_t newSocketObjOps = {
    .open = socket_family_new_open,
    .cleanup = socket_family_new_cleanup,
};

uint64_t socket_family_register(socket_family_t* family)
{
    if (!family || !family->name)
    {
        LOG_ERR("socket_family_register: invalid family\n");
        return ERROR(EINVAL);
    }

    if (!family->init || !family->deinit || !family->bind || !family->listen || !family->accept || !family->connect ||
        !family->send || !family->receive || !family->poll)
    {
        LOG_ERR("socket_family_register: '%s' has unimplemented operations\n", family->name);
        return ERROR(EINVAL);
    }

    if (sysdir_init(&family->dir, "/net", family->name, family) == ERR)
    {
        LOG_ERR("socket_family_register: failed to create directory sys:/net/%s\n", family->name);
        return ERR;
    }
    if (sysobj_init(&family->newObj, &family->dir, "new", &newSocketObjOps, NULL) == ERR)
    {
        sysdir_deinit(&family->dir, NULL);
        LOG_ERR("socket_family_register: failed to create 'new' object for family '%s'\n", family->name);
        return ERR;
    }

    return 0;
}
