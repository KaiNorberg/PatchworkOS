#include "socket_family.h"

#include "defs.h"
#include "sched.h"
#include "socket.h"
#include "sysfs.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/math.h>

static atomic_uint64 newId = ATOMIC_VAR_INIT(0);

static uint64_t socket_family_new_read(file_t* file, void* buffer, uint64_t count)
{
    const char* id = file->private;
    uint64_t len = strlen(id);

    count = (file->pos <= len) ? MIN(count, len - file->pos) : 0;
    memcpy(buffer, id + file->pos, count);
    file->pos += count;

    return 0;
}

static uint64_t socket_family_new_seek(file_t* file, int64_t offset, seek_origin_t origin)
{
    char* id = file->private;
    uint64_t len = strlen(id);

    uint64_t position;
    switch (origin)
    {
    case SEEK_SET:
    {
        position = offset;
    }
    break;
    case SEEK_CUR:
    {
        position = file->pos + offset;
    }
    break;
    case SEEK_END:
    {
        position = len - offset;
    }
    break;
    default:
    {
        position = 0;
    }
    break;
    }

    file->pos = MIN(position, len);
    return position;
}

static file_ops_t familyNewFileOps =
{
    .read = socket_family_new_read,
    .seek = socket_family_new_seek,
};

static file_t* socket_family_new_open(volume_t* volume, resource_t* resource)
{
    socket_family_t* family = resource->dir->private;

    socket_handle_t* handle = malloc(sizeof(socket_handle_t));
    ulltoa(atomic_fetch_add(&newId, 1), handle->id, 10);
    handle->dir = socket_create(family, handle->id);
    if (handle->dir == NULL)
    {
        free(handle);
        return NULL;
    }

    file_t* file = file_new(volume);
    if (file == NULL)
    {
        sysdir_free(handle->dir);
        free(handle);
        return NULL;
    }
    file->ops = &familyNewFileOps;
    file->private = handle;
    return file;
}

static void socket_family_new_cleanup(resource_t* resource, file_t* file)
{
    socket_handle_t* handle = file->private;
    sysdir_free(handle->dir);
    free(handle);
}

static resource_ops_t familyNewResOps =
{
    .open = socket_family_new_open,
    .cleanup = socket_family_new_cleanup,
};

static void socket_family_on_free(sysdir_t* sysdir)
{
    // Do nothing
}

sysdir_t* socket_family_expose(socket_family_t* family)
{
    if (family->init == NULL || family->deinit == NULL)
    {
        return ERRPTR(EINVAL);
    }

    sysdir_t* sysdir = sysdir_new("/net", family->name, socket_family_on_free, family);
    if (sysdir == NULL)
    {
        return NULL;
    }

    if (sysdir_add(sysdir, "new", &familyNewResOps, NULL) == ERR)
    {
        sysdir_free(sysdir);
        return NULL;
    }

    return sysdir;
}
