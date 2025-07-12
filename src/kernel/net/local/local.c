#include "local.h"

#include "fs/path.h"
#include "fs/sysfs.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "net/socket_type.h"
#include "sched/wait.h"
#include "net/socket_family.h"
#include "net/socket.h"
#include "net/local/local_conn.h"
#include "net/local/local_listen.h"
#include "sync/lock.h"

#include <_internal/CONTAINER_OF.h>
#include <_internal/MAX_NAME.h>
#include <string.h>
#include <sys/list.h>

static uint64_t local_socket_init(socket_t* sock)
{
    local_socket_data_t* data = heap_alloc(sizeof(local_socket_data_t), HEAP_NONE);
    if (data == NULL)
    {
        return ERR;
    }
    memset(data, 0, sizeof(local_socket_data_t));
    lock_init(&data->lock);
    sock->private = data;
    return 0;
}

static void local_socket_deinit(socket_t* sock)
{
    local_socket_data_t* data = sock->private;
    if (data == NULL)
    {
        return;
    }

    lock_acquire(&sock->lock);
    lock_acquire(&data->lock);

    switch (sock->currentState)
    {
    case SOCKET_LISTENING:
        if (data->listen.listen != NULL)
        {
            atomic_store(&data->listen.listen->isClosed, true);
            wait_unblock(&data->listen.listen->waitQueue, UINT64_MAX);
            local_listen_deref(data->listen.listen);
            data->listen.listen = NULL;
        }
        break;
    case SOCKET_CONNECTED:
        if (data->conn.conn != NULL)
        {
            atomic_store(&data->conn.conn->isClosed, true);
            wait_unblock(&data->conn.conn->waitQueue, UINT64_MAX);
            local_conn_deref(data->conn.conn);
            data->conn.conn = NULL;
        }
        break;
    default:
        break;
    }

    lock_release(&data->lock);
    lock_release(&sock->lock);

    heap_free(data);
    sock->private = NULL;
}

static uint64_t local_socket_bind(socket_t* sock, const char* address)
{
    if (sock == NULL || address == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    local_socket_data_t* data = sock->private;

    LOCK_SCOPE(&data->lock);

    strncpy(data->bound.address, address, MAX_NAME);
    data->bound.address[MAX_NAME - 1] = '\0';

    return 0;
}

static uint64_t local_socket_listen(socket_t* sock, uint32_t backlog)
{
    if (sock == NULL || backlog == 0)
    {
        errno = EINVAL;
        return ERR;
    }
    local_socket_data_t* data = sock->private;

    LOCK_SCOPE(&data->lock);

    local_listen_t* listen = local_listen_new(data->bound.address, backlog);
    if (listen == NULL)
    {
        return ERR;
    }

    data->listen.listen = listen;
    return 0;
}

static uint64_t local_socket_accept(socket_t* sock, socket_t* newSock)
{
    if (sock == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    local_socket_data_t* data = sock->private;

    LOCK_SCOPE(&data->lock);

    local_listen_t* listen = data->listen.listen;
    if (listen == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_conn_t* conn = NULL;
    while (true)
    {
        LOCK_SCOPE(&listen->lock);

        if (atomic_load(&listen->isClosed))
        {
            errno = ECONNABORTED;
            return ERR;
        }

        if (!list_is_empty(&listen->backlog))
        {
            conn = local_conn_ref(CONTAINER_OF(list_pop(&listen->backlog), local_conn_t, entry));
            break;
        }

        if (sock->flags & PATH_NONBLOCK)
        {
            errno = EWOULDBLOCK;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&listen->waitQueue, &listen->lock, atomic_load(&listen->isClosed) || !list_is_empty(&listen->backlog)) != WAIT_NORM)
        {
            errno = EINTR;
            return ERR;
        }
    }


    local_socket_data_t* newData = newSock->private;

    return 0;
}

static socket_family_t family =
{
    .name = "local",
    .supportedTypes = SOCKET_SEQPACKET,
    .init = local_socket_init,
    .deinit = local_socket_deinit,
    .bind = local_socket_bind,
    .listen = local_socket_listen,
    .connect = local_socket_connect,
    .accept = local_socket_accept,
    .close = local_socket_close,
    .send = local_socket_send,
    .recv = local_socket_recv,
    .poll = local_socket_poll,
};

void net_local_init(void)
{
    if (socket_family_register(&family) == ERR)
    {
        panic(NULL, "Failed to register local socket family");
    }
}
