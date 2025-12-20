#include "socket.h"

#include "socket_family.h"
#include <kernel/fs/ctl.h>
#include <kernel/fs/file.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/sysfs.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rwmutex.h>

#include <errno.h>
#include <kernel/utils/ref.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

static socket_t* socket_get(file_t* file)
{
    return file->path.dentry->parent->inode->private;
}

static socket_t* socket_new(socket_family_t* family, socket_type_t type)
{
    if (family == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    socket_t* sock = calloc(1, sizeof(socket_t));
    if (sock == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    snprintf(sock->id, sizeof(sock->id), "%llu", atomic_fetch_add(&family->newId, 1));
    sock->family = family;
    sock->type = type;
    sock->private = NULL;
    sock->state = SOCKET_NEW;
    mutex_init(&sock->mutex);
    list_init(&sock->files);

    if (family->ops->init(sock) == ERR)
    {
        mutex_deinit(&sock->mutex);
        free(sock);
        return NULL;
    }

    return sock;
}

static void socket_free(socket_t* sock)
{
    if (sock == NULL)
    {
        return;
    }

    if (sock->family != NULL)
    {
        sock->family->ops->deinit(sock);
    }

    mutex_deinit(&sock->mutex);
    free(sock);
}

static uint64_t socket_data_read(file_t* file, void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = socket_get(file);
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->recv == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_CONNECTED)
    {
        errno = ENOTCONN;
        return ERR;
    }

    return sock->family->ops->recv(sock, buf, count, offset, file->mode);
}

static uint64_t socket_data_write(file_t* file, const void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = socket_get(file);
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->send == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_CONNECTED)
    {
        errno = ENOTCONN;
        return ERR;
    }

    return sock->family->ops->send(sock, buf, count, offset, file->mode);
}

static wait_queue_t* socket_data_poll(file_t* file, poll_events_t* revents)
{
    socket_t* sock = socket_get(file);
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->poll == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    MUTEX_SCOPE(&sock->mutex);
    return sock->family->ops->poll(sock, revents);
}

static file_ops_t dataOps = {
    .read = socket_data_read,
    .write = socket_data_write,
    .poll = socket_data_poll,
};

static uint64_t socket_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    (void)argc; // Unused

    socket_t* sock = socket_get(file);
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->bind == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_NEW)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->ops->bind(sock, argv[1]) == ERR)
    {
        return ERR;
    }

    sock->state = SOCKET_BOUND;
    return 0;
}

static uint64_t socket_ctl_listen(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* sock = socket_get(file);
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->listen == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    uint32_t backlog = 128;
    if (argc > 1)
    {
        backlog = atol(argv[1]);
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_BOUND)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->ops->listen(sock, backlog) == ERR)
    {
        return ERR;
    }

    sock->state = SOCKET_LISTENING;
    return 0;
}

static uint64_t socket_ctl_connect(file_t* file, uint64_t argc, const char** argv)
{
    (void)argc; // Unused

    socket_t* sock = socket_get(file);
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->connect == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    if (sock->state != SOCKET_NEW)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->ops->connect(sock, argv[1]) == ERR)
    {
        return ERR;
    }

    sock->state = SOCKET_CONNECTED;
    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps,
    {
        {"bind", socket_ctl_bind, 2, 2},
        {"listen", socket_ctl_listen, 1, 2},
        {"connect", socket_ctl_connect, 2, 2},
        {0},
    });

static uint64_t socket_accept_open(file_t* file)
{
    socket_t* sock = socket_get(file);
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->accept == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (sock->state != SOCKET_LISTENING)
    {
        errno = EINVAL;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);

    socket_t* newSock = socket_new(sock->family, sock->type);
    if (newSock == NULL)
    {
        return ERR;
    }

    if (sock->family->ops->accept(sock, newSock, file->mode) == ERR)
    {
        socket_free(newSock);
        return ERR;
    }

    newSock->state = SOCKET_CONNECTED;
    file->private = newSock;
    return 0;
}

static void socket_accept_close(file_t* file)
{
    socket_t* sock = file->private;
    if (sock == NULL)
    {
        return;
    }

    socket_free(sock);
}

static uint64_t socket_accept_read(file_t* file, void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->recv == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);
    return sock->family->ops->recv(sock, buf, count, offset, file->mode);
}

static uint64_t socket_accept_write(file_t* file, const void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->send == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    MUTEX_SCOPE(&sock->mutex);
    return sock->family->ops->send(sock, buf, count, offset, file->mode);
}

static wait_queue_t* socket_accept_poll(file_t* file, poll_events_t* revents)
{
    socket_t* sock = file->private;
    assert(sock != NULL);
    assert(sock->family != NULL);

    if (sock->family->ops->poll == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    MUTEX_SCOPE(&sock->mutex);
    return sock->family->ops->poll(sock, revents);
}

static file_ops_t acceptOps = {
    .open = socket_accept_open,
    .close = socket_accept_close,
    .read = socket_accept_read,
    .write = socket_accept_write,
    .poll = socket_accept_poll,
};

static void socket_dir_cleanup(inode_t* inode)
{
    socket_t* sock = inode->private;
    if (sock == NULL)
    {
        return;
    }

    socket_free(sock);
}

static inode_ops_t dirInodeOps = {
    .cleanup = socket_dir_cleanup,
};

static void socket_unmount(superblock_t* superblock)
{
    if (superblock == NULL)
    {
        return;
    }

    socket_t* sock = superblock->root->inode->private;
    if (sock == NULL)
    {
        return;
    }

    MUTEX_SCOPE(&sock->mutex);

    while (!list_is_empty(&sock->files))
    {
        UNREF(CONTAINER_OF_SAFE(list_pop_first(&sock->files), dentry_t, otherEntry));
    }
}

static superblock_ops_t superblockOps = {
    .unmount = socket_unmount,
};

static sysfs_file_desc_t files[] = {
    {.name = "ctl", .inodeOps = NULL, .fileOps = &ctlOps},
    {.name = "data", .inodeOps = NULL, .fileOps = &dataOps},
    {.name = "accept", .inodeOps = NULL, .fileOps = &acceptOps},
    {.name = NULL},
};

uint64_t socket_create(socket_family_t* family, socket_type_t type, char* out, uint64_t outSize)
{
    if (family == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    socket_t* sock = socket_new(family, type);
    if (sock == NULL)
    {
        return ERR;
    }

    path_t familyDir = socket_family_get_dir(family);
    PATH_DEFER(&familyDir);

    mount_t* mount = sysfs_submount_new(&familyDir, sock->id, NULL, MOUNT_PROPAGATE_CHILDREN,
        MODE_DIRECTORY | MODE_ALL_PERMS, &dirInodeOps, &superblockOps, sock);
    if (mount == NULL)
    {
        socket_free(sock);
        return ERR;
    }
    UNREF_DEFER(mount);

    if (sysfs_files_create(mount->source, files, NULL, &sock->files) == ERR)
    {
        socket_free(sock);
        return ERR;
    }

    strncpy_s(out, outSize, sock->id, MAX_NAME - 1);
    out[outSize - 1] = '\0';
    return 0;
}