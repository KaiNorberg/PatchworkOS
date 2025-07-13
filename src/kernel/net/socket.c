#include "socket.h"

#include "fs/ctl.h"
#include "fs/path.h"
#include "mem/heap.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "socket_family.h"
#include "sync/lock.h"
#include "sync/mutex.h"
#include "sync/rwmutex.h"

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

    file->private = sock;
    return 0;
}

static uint64_t socket_data_read(file_t* file, void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->recv == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (sock->flags & PATH_NONBLOCK)
    {
        if (rwmutex_read_try_acquire(&sock->mutex) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (rwmutex_read_acquire(&sock->mutex) == ERR)
        {
            return ERR;
        }
    }

    if (sock->currentState != SOCKET_CONNECTED)
    {
        rwmutex_read_release(&sock->mutex);
        errno = ENOTCONN;
        return ERR;
    }

    uint64_t result = sock->family->recv(sock, buf, count, offset);
    rwmutex_read_release(&sock->mutex);
    return result;
}

static uint64_t socket_data_write(file_t* file, const void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->send == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (sock->flags & PATH_NONBLOCK)
    {
        if (rwmutex_read_try_acquire(&sock->mutex) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (rwmutex_read_acquire(&sock->mutex) == ERR)
        {
            return ERR;
        }
    }

    if (sock->currentState != SOCKET_CONNECTED)
    {
        rwmutex_read_release(&sock->mutex);
        errno = ENOTCONN;
        return ERR;
    }

    uint64_t result = sock->family->send(sock, buf, count, offset);
    rwmutex_read_release(&sock->mutex);
    return result;
}

static wait_queue_t* socket_data_poll(file_t* file, poll_events_t events, poll_events_t* revents)
{
    socket_t* sock = file->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (sock->family->poll == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    return sock->family->poll(sock, events, revents);
}

static file_ops_t dataOps = {
    .open = socket_data_open,
    .read = socket_data_read,
    .write = socket_data_write,
    .poll = socket_data_poll,
};

static uint64_t socket_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* sock = file->inode->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->bind == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (socket_start_transition(sock, SOCKET_BOUND) == ERR)
    {
        return ERR;
    }

    uint64_t result = sock->family->bind(sock, argv[1]);
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

    if (sock->family->listen == NULL)
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

    uint64_t result = sock->family->listen(sock, backlog);
    socket_end_transition(sock, result);
    return result;
}

static uint64_t socket_ctl_connect(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* sock = file->inode->private;
    if (sock == NULL || sock->family == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->family->connect == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (socket_start_transition(sock, SOCKET_CONNECTING) == ERR)
    {
        return ERR;
    }

    uint64_t result = sock->family->connect(sock, argv[1]);
    if (result == ERR)
    {
        socket_end_transition(sock, ERR);
        return result;
    }

    bool notFinished = (sock->flags & PATH_NONBLOCK) && (result == ERR && errno == EINPROGRESS);
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

    if (sock->family->accept == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (sock->flags & PATH_NONBLOCK)
    {
        if (rwmutex_read_try_acquire(&sock->mutex) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (rwmutex_read_acquire(&sock->mutex) == ERR)
        {
            return ERR;
        }
    }

    if (sock->currentState != SOCKET_LISTENING)
    {
        rwmutex_read_release(&sock->mutex);
        errno = EINVAL;
        return ERR;
    }

    socket_t* newSock = socket_new(sock->family, sock->type, file->flags);
    if (newSock == NULL)
    {
        rwmutex_read_release(&sock->mutex);
        return ERR;
    }

    if (socket_start_transition(newSock, SOCKET_CONNECTING) == ERR)
    {
        rwmutex_read_release(&sock->mutex);
        return ERR;
    }

    if (sock->family->accept(sock, newSock) == ERR)
    {
        socket_end_transition(newSock, ERR);
        socket_free(newSock);
        rwmutex_read_release(&sock->mutex);
        return ERR;
    }

    socket_continue_transition(newSock, SOCKET_CONNECTED);

    socket_end_transition(newSock, 0);
    file->private = newSock;
    rwmutex_read_release(&sock->mutex);
    return 0;
}

static file_ops_t acceptOps = {
    .open = socket_accept_open,
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

    if (sock->family != NULL && sock->family->deinit != NULL)
    {
        sock->family->deinit(sock);
    }

    rwmutex_deinit(&sock->mutex);
    heap_free(sock);
}

static inode_ops_t dirInodeOps = {
    .cleanup = socket_inode_cleanup,
};

socket_t* socket_new(socket_family_t* family, socket_type_t type, path_flags_t flags)
{
    if (family == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    socket_t* sock = heap_alloc(sizeof(socket_t), HEAP_NONE);
    if (sock == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    uint64_t id = atomic_fetch_add(&family->newId, 1);
    snprintf(sock->id, sizeof(sock->id), "%llu", id);
    memset(sock->address, 0, MAX_NAME);
    sock->family = family;
    sock->type = type;
    sock->flags = flags;
    sock->creator = sched_process()->id;
    sock->private = NULL;
    rwmutex_init(&sock->mutex);
    sock->currentState = SOCKET_NEW;
    sock->nextState = SOCKET_NEW;

    if (family->init(sock) == ERR)
    {
        rwmutex_deinit(&sock->mutex);
        heap_free(sock);
        return NULL;
    }

    if (sysfs_dir_init(&sock->dir, &family->dir, sock->id, &dirInodeOps, sock) == ERR)
    {
        goto error;
    }

    if (sysfs_file_init(&sock->ctlFile, &sock->dir, "ctl", NULL, &ctlOps, sock) == ERR)
    {
        goto error;
    }

    if (sysfs_file_init(&sock->dataFile, &sock->dir, "data", NULL, &dataOps, sock) == ERR)
    {
        sysfs_file_deinit(&sock->ctlFile);
        goto error;
    }

    if (sysfs_file_init(&sock->acceptFile, &sock->dir, "accept", NULL, &acceptOps, sock) == ERR)
    {
        sysfs_file_deinit(&sock->ctlFile);
        sysfs_file_deinit(&sock->dataFile);
        goto error;
    }

    return sock;

error:
    family->deinit(sock);
    sysfs_dir_deinit(&sock->dir);
    heap_free(sock);
    return NULL;
}

void socket_free(socket_t* sock)
{
    if (sock == NULL)
    {
        return;
    }

    sysfs_file_deinit(&sock->ctlFile);
    sysfs_file_deinit(&sock->dataFile);
    sysfs_file_deinit(&sock->acceptFile);
    sysfs_dir_deinit(&sock->dir);
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

    if (sock->flags & PATH_NONBLOCK)
    {
        if (rwmutex_write_try_acquire(&sock->mutex) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (rwmutex_write_acquire(&sock->mutex) == ERR)
        {
            return ERR;
        }
    }

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
