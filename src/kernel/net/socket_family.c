#include "socket_family.h"

#include "defs.h"
#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "sched/thread.h"
#include "socket.h"
#include "utils/log.h"

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

static file_ops_t familyNewFileOps = {
    .read = socket_family_new_read,
    .seek = socket_family_new_seek,
};

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
    file->ops = &familyNewFileOps;
    file->private = socket;
    return file;
}

static void socket_family_new_cleanup(sysobj_t* sysobj, file_t* file)
{
    socket_t* socket = file->private;
    socket_free(socket);
}

static sysobj_ops_t familyNewObjOps = {
    .open = socket_family_new_open,
    .cleanup = socket_family_new_cleanup,
};

uint64_t socket_family_expose(socket_family_t* family)
{
    if (family->init == NULL || family->deinit == NULL)
    {
        return ERROR(EINVAL);
    }

    assert(sysdir_init(&family->dir, "/net", family->name, family) != ERR);
    assert(sysobj_init(&family->newObj, &family->dir, "new", &familyNewObjOps, NULL) != ERR);

    return 0;
}
