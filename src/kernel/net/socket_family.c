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

static uint64_t socket_family_new_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    socket_t* socket = file->private;

    uint64_t len = strlen(socket->id);
    return BUFFER_READ(buffer, count, offset, socket->id, len + 1); // Include null terminator
}

static uint64_t socket_family_new_open(file_t* file)
{
    socket_family_t* family = file->dentry->inode->private;

    socket_t* socket = socket_new(family, file->flags);
    if (socket == NULL)
    {
        return ERR;
    }

    file->private = socket;
    return 0;
}

static void socket_family_new_cleanup(file_t* file)
{
    socket_t* socket = file->private;
    if (socket != NULL)
    {
        socket_free(socket);
    }
}

static file_ops_t newOps = {
    .open = socket_family_new_open,
    .read = socket_family_new_read,
    .cleanup = socket_family_new_cleanup,
};

uint64_t socket_family_register(socket_family_t* family)
{
    if (!family || !family->name)
    {
        LOG_ERR("socket_family_register: invalid family\n");
        errno = EINVAL;
        return ERR;
    }

    if (!family->init || !family->deinit || !family->bind || !family->listen || !family->accept || !family->connect ||
        !family->send || !family->receive || !family->poll)
    {
        LOG_ERR("socket_family_register: '%s' has unimplemented operations\n", family->name);
        errno = EINVAL;
        return ERR;
    }

    if (sysdir_init(&family->dir, "/net", family->name, family) == ERR)
    {
        LOG_ERR("socket_family_register: failed to create directory /dev/net/%s\n", family->name);
        return ERR;
    }
    if (sysfile_init(&family->newFile, &family->dir, "new", &newOps, NULL) == ERR)
    {
        sysdir_deinit(&family->dir, NULL);
        LOG_ERR("socket_family_register: failed to create 'new' object for family '%s'\n", family->name);
        return ERR;
    }

    return 0;
}
