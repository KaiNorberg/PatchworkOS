#include "socket.h"

#include "socket_family.h"
#include <kernel/fs/ctl.h>
#include <kernel/fs/file.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/sched/process.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rwmutex.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/io.h>

static uint64_t socket_data_open(file_t* file)
{
    socket_t* sock = file->inode->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    file->private = REF(sock);
    return 0;
}

static void socket_data_close(file_t* file)
{
    socket_t* sock = file->private;
    if (sock == NULL)
    {
        return;
    }

    UNREF(sock);
}

static uint64_t socket_data_read(file_t* file, void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->ops->recv == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    RWMUTEX_READ_SCOPE(&sock->mutex);

    if (sock->currentState != SOCKET_CONNECTED)
    {
        errno = ENOTCONN;
        return ERR;
    }

    return sock->family->ops->recv(sock, buf, count, offset, file->mode);
}

static uint64_t socket_data_write(file_t* file, const void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->ops->send == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    RWMUTEX_READ_SCOPE(&sock->mutex);

    if (sock->currentState != SOCKET_CONNECTED)
    {
        errno = ENOTCONN;
        return ERR;
    }

    return sock->family->ops->send(sock, buf, count, offset, file->mode);
}

static wait_queue_t* socket_data_poll(file_t* file, poll_events_t* revents)
{
    socket_t* sock = file->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (sock->family->ops->poll == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    return sock->family->ops->poll(sock, revents);
}

static file_ops_t dataOps = {
    .open = socket_data_open,
    .close = socket_data_close,
    .read = socket_data_read,
    .write = socket_data_write,
    .poll = socket_data_poll,
};

static uint64_t socket_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    (void)argc; // Unused

    socket_t* sock = file->inode->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->ops->bind == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (socket_start_transition(sock, SOCKET_BOUND) == ERR)
    {
        return ERR;
    }

    uint64_t result = sock->family->ops->bind(sock, argv[1]);
    socket_end_transition(sock, result);
    return result;
}

static uint64_t socket_ctl_listen(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* sock = file->inode->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

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

    if (socket_start_transition(sock, SOCKET_LISTENING) == ERR)
    {
        return ERR;
    }

    uint64_t result = sock->family->ops->listen(sock, backlog);
    socket_end_transition(sock, result);
    return result;
}

static uint64_t socket_ctl_connect(file_t* file, uint64_t argc, const char** argv)
{
    (void)argc; // Unused

    socket_t* sock = file->inode->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->ops->connect == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (socket_start_transition(sock, SOCKET_CONNECTING) == ERR)
    {
        return ERR;
    }

    uint64_t result = sock->family->ops->connect(sock, argv[1]);
    if (result == ERR)
    {
        socket_end_transition(sock, ERR);
        return result;
    }

    bool notFinished = (file->mode & MODE_NONBLOCK) && (result == ERR && errno == EINPROGRESS);
    if (notFinished) // Non blocking and not yet connected.
    {
        socket_end_transition(sock, 0);

        errno = EINPROGRESS;
        return ERR;
    }

    // Connection finished immediately.
    socket_continue_transition(sock, SOCKET_CONNECTED);

    socket_end_transition(sock, 0);
    return 0;
}

CTL_STANDARD_OPS_DEFINE(ctlOps,
    (ctl_array_t){
        {"bind", socket_ctl_bind, 2, 2},
        {"listen", socket_ctl_listen, 1, 2},
        {"connect", socket_ctl_connect, 2, 2},
        {0},
    });

static uint64_t socket_accept_open(file_t* file)
{
    socket_t* sock = file->inode->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->ops->accept == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    RWMUTEX_READ_SCOPE(&sock->mutex);

    if (sock->currentState != SOCKET_LISTENING)
    {
        errno = EINVAL;
        return ERR;
    }

    socket_t* newSock = socket_new(sock->family, sock->type);
    if (newSock == NULL)
    {
        return ERR;
    }

    if (socket_start_transition(newSock, SOCKET_CONNECTING) == ERR)
    {
        return ERR;
    }

    if (sock->family->ops->accept(sock, newSock, file->mode) == ERR)
    {
        socket_end_transition(newSock, ERR);
        UNREF(newSock);
        return ERR;
    }

    socket_continue_transition(newSock, SOCKET_CONNECTED);

    socket_end_transition(newSock, 0);
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

    UNREF(sock);
}

static file_ops_t acceptOps = {
    .open = socket_accept_open,
    .close = socket_accept_close,
    .read = socket_data_read,
    .write = socket_data_write,
    .poll = socket_data_poll,
};

static void socket_inode_cleanup(inode_t* inode)
{
    socket_t* sock = inode->private;
    if (sock == NULL)
    {
        return;
    }

    UNREF(sock);
}

static inode_ops_t inodeOps = {
    .cleanup = socket_inode_cleanup,
};

/**
 * Will only be called when the socket reference count reaches 0. Meaning the socket must be unmounted first.
 */
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

    rwmutex_deinit(&sock->mutex);
    free(sock);
}

static void socket_unmount(superblock_t* superblock)
{
    if (superblock == NULL)
    {
        return;
    }

    socket_t* sock = superblock->private;
    if (sock == NULL)
    {
        return;
    }
    UNREF(sock->ctlFile);
    UNREF(sock->dataFile);
    UNREF(sock->acceptFile);
    UNREF(sock);
    superblock->private = NULL;
}

static superblock_ops_t superblockOps = {
    .unmount = socket_unmount,
};

socket_t* socket_new(socket_family_t* family, socket_type_t type)
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

    ref_init(&sock->ref, socket_free);
    snprintf(sock->id, sizeof(sock->id), "%llu", atomic_fetch_add(&family->newId, 1));
    sock->family = family;
    sock->type = type;
    sock->private = NULL;
    rwmutex_init(&sock->mutex);
    sock->currentState = SOCKET_NEW;
    sock->nextState = SOCKET_NEW;

    if (family->ops->init(sock) == ERR)
    {
        rwmutex_deinit(&sock->mutex);
        free(sock);
        return NULL;
    }

    path_t familyDir = PATH_EMPTY;
    if (socket_family_get_dir(family, &familyDir) == ERR)
    {
        family->ops->deinit(sock);
        rwmutex_deinit(&sock->mutex);
        free(sock);
        return NULL;
    }

    mount_t* mount =
        sysfs_mount_new(&familyDir, sock->id, NULL, MOUNT_PROPAGATE_CHILDREN, MODE_ALL_PERMS, &superblockOps);
    path_put(&familyDir);
    if (mount == NULL)
    {
        family->ops->deinit(sock);
        rwmutex_deinit(&sock->mutex);
        free(sock);
        return NULL;
    }
    mount->superblock->private = REF(sock);
    UNREF(mount);

    sock->ctlFile = sysfs_file_new(mount->root, "ctl", &inodeOps, &ctlOps, REF(sock));
    if (sock->ctlFile == NULL)
    {
        family->ops->deinit(sock);
        UNREF(mount->superblock);
        rwmutex_deinit(&sock->mutex);
        free(sock);
        return NULL;
    }

    sock->dataFile = sysfs_file_new(mount->root, "data", &inodeOps, &dataOps, REF(sock));
    if (sock->dataFile == NULL)
    {
        family->ops->deinit(sock);
        UNREF(mount->superblock);
        UNREF(sock->ctlFile);
        rwmutex_deinit(&sock->mutex);
        free(sock);
        return NULL;
    }

    sock->acceptFile = sysfs_file_new(mount->root, "accept", &inodeOps, &acceptOps, REF(sock));
    if (sock->acceptFile == NULL)
    {
        family->ops->deinit(sock);
        UNREF(mount->superblock);
        UNREF(sock->ctlFile);
        UNREF(sock->dataFile);
        rwmutex_deinit(&sock->mutex);
        free(sock);
        return NULL;
    }

    return sock;
}

static const bool validTransitions[SOCKET_STATE_AMOUNT][SOCKET_STATE_AMOUNT] = {
    [SOCKET_NEW] =
        {
            [SOCKET_BOUND] = true,
            [SOCKET_CONNECTING] = true,
            [SOCKET_CLOSED] = true,
        },
    [SOCKET_BOUND] =
        {
            [SOCKET_LISTENING] = true,
            [SOCKET_CONNECTING] = true,
            [SOCKET_CONNECTED] = true,
            [SOCKET_CLOSED] = true,
        },
    [SOCKET_LISTENING] =
        {
            [SOCKET_CONNECTED] = true,
            [SOCKET_CLOSED] = true,
        },
    [SOCKET_CONNECTING] =
        {
            [SOCKET_CONNECTED] = true,
        },
    [SOCKET_CONNECTED] =
        {
            [SOCKET_CLOSING] = true,
        },
    [SOCKET_CLOSING] =
        {
            [SOCKET_CLOSED] = true,
        },
    [SOCKET_CLOSED] = {},
};

bool socket_can_transition(socket_state_t from, socket_state_t to)
{
    if (from >= SOCKET_STATE_AMOUNT || to >= SOCKET_STATE_AMOUNT || from < 0 || to < 0)
    {
        return false;
    }
    return validTransitions[from][to];
}

uint64_t socket_start_transition(socket_t* sock, socket_state_t state)
{
    if (sock == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    rwmutex_write_acquire(&sock->mutex);

    if (!socket_can_transition(sock->currentState, state))
    {
        rwmutex_write_release(&sock->mutex);
        errno = EINVAL;
        return ERR;
    }

    if (sock->currentState == state)
    {
        rwmutex_write_release(&sock->mutex);
        errno = EINVAL;
        return ERR;
    }

    sock->nextState = state;
    return 0;
}

void socket_continue_transition(socket_t* sock, socket_state_t state)
{
    sock->currentState = sock->nextState;

    assert(socket_can_transition(sock->currentState, state)); // This should always pass.

    sock->nextState = state;
}

void socket_end_transition(socket_t* sock, uint64_t result)
{
    if (result != ERR)
    {
        sock->currentState = sock->nextState;
    }

    sock->nextState = sock->currentState;

    rwmutex_write_release(&sock->mutex);
}
