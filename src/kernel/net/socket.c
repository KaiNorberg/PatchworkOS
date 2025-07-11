#include "socket.h"

#include "fs/ctl.h"
#include "fs/path.h"
#include "mem/heap.h"
#include "sched/sched.h"
#include "proc/process.h"
#include "sched/wait.h"
#include "socket_family.h"
#include "sync/lock.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t socket_data_read(file_t* file, void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    if (sock->family->recv == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    lock_acquire(&sock->lock);
    if (sock->currentState != SOCKET_CONNECTED)
    {
        lock_release(&sock->lock);
        errno = ENOTCONN;
        return ERR;
    }
    lock_release(&sock->lock);

    return sock->family->recv(sock, buf, count, offset);
}

static uint64_t socket_data_write(file_t* file, const void* buf, size_t count, uint64_t* offset)
{
    socket_t* sock = file->private;
    if (sock->family->send == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    lock_acquire(&sock->lock);
    if (sock->currentState != SOCKET_CONNECTED)
    {
        lock_release(&sock->lock);
        errno = ENOTCONN;
        return ERR;
    }
    lock_release(&sock->lock);

    return sock->family->send(sock, buf, count, offset);
}

static wait_queue_t* socket_data_poll(file_t* file, poll_events_t events, poll_events_t* occoured)
{
    socket_t* sock = file->private;
    if (sock->family->poll == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    return sock->family->poll(sock, events, occoured);
}

static file_ops_t dataOps = {
    .read = socket_data_read,
    .write = socket_data_write,
    .poll = socket_data_poll,
};

static uint64_t socket_ctl_bind(file_t* file, uint64_t argc, const char** argv)
{
    socket_t* sock = file->private;
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
    socket_t* sock = file->private;
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
    socket_t* sock = file->private;
    if (sock->family->connect == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (argc != 2)
    {
        errno = EINVAL;
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

    // TODO: I am unsure of this logic, currently i dont have any trully blocking sockets, TPC, etc, so further testing is needed.

    bool notFinished = (sock->flags & PATH_NONBLOCK) && (result == ERR && errno == EINPROGRESS);
    if (notFinished) // Non blocking and not yet connected, check this stuff again later.
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

CTL_STANDARD_WRITE_DEFINE(socket_ctl_write,
    (ctl_array_t){
        {"bind", socket_ctl_bind, 2, 2},
        {"listen", socket_ctl_listen, 1, 2},
        {"connect", socket_ctl_connect, 2, 2},
        {0},
    });

static file_ops_t ctlOps = {
    .write = socket_ctl_write,
};

static uint64_t socket_accept_open(file_t* file)
{
    socket_t* sock = file->inode->private;
    if (sock->family->accept == NULL)
    {
        errno = ENOSYS;
        return ERR;
    }

    lock_acquire(&sock->lock);
    if (sock->currentState != SOCKET_LISTENING)
    {
        lock_release(&sock->lock);
        errno = EINVAL;
        return ERR;
    }
    lock_release(&sock->lock);

    socket_t* newSock = socket_new(sock->family, sock->type, file->flags);
    if (newSock == NULL)
    {
        return ERR;
    }

    if (socket_start_transition(newSock, SOCKET_CONNECTING) == ERR)
    {
        return ERR;
    }

    if (sock->family->accept(sock, newSock) == ERR)
    {
        socket_end_transition(newSock, ERR);
        socket_free(newSock);
        return ERR;
    }

    socket_continue_transition(newSock, SOCKET_CONNECTED);

    socket_end_transition(newSock, 0);
    file->private = newSock;
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

    if (sock->family != NULL && sock->family->deinit != NULL)
    {
        sock->family->deinit(sock);
    }

    wait_queue_deinit(&sock->waitQueue);
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
    wait_queue_init(&sock->waitQueue);
    sock->currentState = SOCKET_NEW;
    sock->nextState = SOCKET_NEW;
    sock->isTransitioning = false;
    lock_init(&sock->lock);

    if (family->init(sock) == ERR)
    {
        wait_queue_deinit(&sock->waitQueue);
        heap_free(sock);
        return NULL;
    }

    if (sysfs_dir_init(&sock->dir, &family->dir, sock->id, &dirInodeOps, sock) == ERR)
    {
        family->deinit(sock);
        wait_queue_deinit(&sock->waitQueue);
        heap_free(sock);
        return NULL;
    }

    if (sysfs_file_init(&sock->ctlFile, &sock->dir, "ctl", NULL, &ctlOps, sock) == ERR)
    {
        goto error;
    }

    if (sysfs_file_init(&sock->dataFile, &sock->dir, "data", NULL, &dataOps, sock))
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
    wait_queue_deinit(&sock->waitQueue);
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
    [SOCKET_NEW] = {
        [SOCKET_BOUND] = true,
        [SOCKET_CONNECTING] = true,
        [SOCKET_CLOSED] = true,
    },
    [SOCKET_BOUND] = {
        [SOCKET_LISTENING] = true,
        [SOCKET_CONNECTING] = true,
        [SOCKET_CONNECTED] = true,
        [SOCKET_CLOSED] = true,
    },
    [SOCKET_LISTENING] = {
        [SOCKET_CONNECTED] = true,
        [SOCKET_CLOSED] = true,
    },
    [SOCKET_CONNECTING] = {
        [SOCKET_CONNECTED] = true,
    },
    [SOCKET_CONNECTED] = {
        [SOCKET_CLOSING] = true,
    },
    [SOCKET_CLOSING] = {
        [SOCKET_CLOSED] = true,
    },
    [SOCKET_CLOSED] = {
    }
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

    LOCK_DEFER(&sock->lock);

    if (!socket_can_transition(sock->currentState, state))
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->currentState == state)
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->flags & PATH_NONBLOCK && sock->isTransitioning)
    {
        errno = EWOULDBLOCK;
        return ERR;
    }

    if (WAIT_BLOCK_LOCK(&sock->waitQueue, &sock->lock, !sock->isTransitioning) != WAIT_NORM)
    {
        errno = EINTR;
        return ERR;
    }

    // We cant check nextState before the block as that would cause a race condition, we have to double check after instead.
    if (!socket_can_transition(sock->currentState, state))
    {
        errno = EINVAL;
        return ERR;
    }

    if (sock->currentState == state)
    {
        errno = EINVAL;
        return ERR;
    }

    sock->nextState = state;
    sock->isTransitioning = true;

    return 0;
}

void socket_continue_transition(socket_t* sock, socket_state_t state)
{
    LOCK_DEFER(&sock->lock);

    sock->currentState = sock->nextState;

    assert(sock->isTransitioning);
    assert(socket_can_transition(sock->currentState, state)); // This should always pass.

    sock->nextState = state;
}

void socket_end_transition(socket_t* sock, uint64_t result)
{
    LOCK_DEFER(&sock->lock);

    if (result != ERR)
    {
        sock->currentState = sock->nextState;
    }

    sock->isTransitioning = false;
    sock->nextState = sock->currentState;
    wait_unblock(&sock->waitQueue, WAIT_ALL);
}
