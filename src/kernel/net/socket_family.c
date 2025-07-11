#include "socket_family.h"

#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "net/socket.h"
#include "log/log.h"
#include "net/net.h"
#include <sys/list.h>

static uint64_t socket_family_new_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* sock = file->private;

    uint64_t length = strlen(sock->id);
    return BUFFER_READ(buffer, count, offset, sock->id, length); // Include null terminator
}

static uint64_t socket_family_new_open(file_t* file)
{
    socket_family_t* family = file->inode->private;

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

static void socket_family_new_cleanup(file_t* file)
{
    socket_t* socket = file->private;
    if (socket != NULL)
    {
        socket_free(socket);
    }
}

static file_ops_t newFileOps = {
    .read = socket_family_new_read,
    .open = socket_family_new_open,
    .cleanup = socket_family_new_cleanup,
};

uint64_t socket_family_register(socket_family_t* family)
{
    if (family == NULL || family->name == NULL) {
        errno = EINVAL;
        return ERR;
    }

    if (family->ops.init == NULL || family->ops.deinit == NULL)
    {
        LOG_ERR("socket: family %s missing required operations\n", family->name);
        errno = EINVAL;
        return ERR;
    }

    atomic_init(&family->newId, 0);

    if (sysfs_dir_init(&family->dir, net_get_dir(), family->name, NULL, family) == ERR)
    {
        LOG_ERR("socket: failed to create sysfs dir for family %s\n", family->name);
        return ERR;
    }

    for (uint64_t i = 0; i < SOCKET_TYPE_AMOUNT; i++)
    {
        socket_type_t type = (1 << i);
        if (!(type & family->supportedTypes))
        {
            continue;
        }

        if (sysfs_file_init(&family->newFile, &family->dir, "new", NULL, &newFileOps, family) == ERR)
        {
            LOG_ERR("socket: failed to create sysfs file for family %s\n", family->name);
            sysfs_dir_deinit(&family->dir);
            return ERR;
        }
    }

    LOG_INFO("socket: registered family %s\n", family->name);
    return 0;
}

void socket_family_unregister(socket_family_t* family)
{
    sysfs_dir_deinit(&family->dir);
    LOG_INFO("socket: unregistered family %s\n", family->name);
    return;
}
