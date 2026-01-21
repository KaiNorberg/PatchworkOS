#include "local.h"

#include "local_conn.h"
#include "local_listen.h"

#include <kernel/fs/filesystem.h>
#include <kernel/fs/netfs.h>
#include <kernel/fs/path.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/fifo.h>
#include <kernel/utils/ref.h>

#include <stdlib.h>
#include <sys/fs.h>
#include <sys/list.h>

static local_listen_t* local_socket_get_listen(local_socket_t* data)
{
    if (data->listen == NULL)
    {
        return NULL;
    }

    return REF(data->listen);
}

static local_conn_t* local_socket_get_conn(local_socket_t* data)
{
    if (data->conn == NULL)
    {
        return NULL;
    }

    return REF(data->conn);
}

static uint64_t local_socket_init(socket_t* sock)
{
    if (sock->type != SOCKET_SEQPACKET)
    {
        errno = EINVAL;
        return ERR;
    }

    local_socket_t* data = calloc(1, sizeof(local_socket_t));
    if (data == NULL)
    {
        return ERR;
    }
    sock->data = data;
    return 0;
}

static void local_socket_deinit(socket_t* sock)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return;
    }

    switch (sock->state)
    {
    default:
        break;
    }

    if (data->listen != NULL)
    {
        lock_acquire(&data->listen->lock);
        data->listen->isClosed = true;
        wait_unblock(&data->listen->waitQueue, WAIT_ALL, EOK);
        lock_release(&data->listen->lock);
        UNREF(data->listen);
    }

    if (data->conn != NULL)
    {
        lock_acquire(&data->conn->lock);
        data->conn->isClosed = true;
        wait_unblock(&data->conn->waitQueue, WAIT_ALL, EOK);
        lock_release(&data->conn->lock);
        UNREF(data->conn);
    }

    free(data);
    sock->data = NULL;
}

static uint64_t local_socket_bind(socket_t* sock)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (data->listen != NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_listen_t* listen = local_listen_new(sock->address);
    if (listen == NULL)
    {
        return ERR;
    }

    data->listen = listen;
    return 0;
}

static uint64_t local_socket_listen(socket_t* sock, uint32_t backlog)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_listen_t* listen = data->listen;
    if (listen == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    LOCK_SCOPE(&listen->lock);

    if (backlog < LOCAL_MAX_BACKLOG)
    {
        listen->maxBacklog = backlog;
    }

    listen->isClosed = false;
    return 0;
}

static uint64_t local_socket_connect(socket_t* sock)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (data->conn != NULL)
    {
        errno = EISCONN;
        return ERR;
    }

    local_listen_t* listen = local_listen_find(sock->address);
    if (listen == NULL)
    {
        errno = ECONNREFUSED;
        return ERR;
    }
    UNREF_DEFER(listen);

    local_conn_t* conn = local_conn_new(listen);
    if (conn == NULL)
    {
        return ERR;
    }
    UNREF_DEFER(conn);

    LOCK_SCOPE(&listen->lock);

    if (listen->isClosed)
    {
        errno = ECONNREFUSED;
        return ERR;
    }

    if (listen->pendingAmount >= listen->maxBacklog)
    {
        errno = ECONNREFUSED;
        return ERR;
    }

    listen->pendingAmount++;
    list_push_back(&listen->backlog, &conn->entry);

    wait_unblock(&listen->waitQueue, WAIT_ALL, EOK);

    data->conn = REF(conn);
    data->isServer = false;
    return 0;
}

static uint64_t local_socket_accept(socket_t* sock, socket_t* newSock, mode_t mode)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_listen_t* listen = local_socket_get_listen(data);
    if (listen == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    UNREF_DEFER(listen);

    local_conn_t* conn = NULL;
    while (true)
    {
        LOCK_SCOPE(&listen->lock);

        if (listen->isClosed)
        {
            errno = ECONNABORTED;
            return ERR;
        }

        if (!list_is_empty(&listen->backlog))
        {
            list_entry_t* entry = list_pop_front(&listen->backlog);
            local_conn_t* container = CONTAINER_OF(entry, local_conn_t, entry);
            conn = REF(container);
            listen->pendingAmount--;
            break;
        }

        if (mode & MODE_NONBLOCK)
        {
            errno = EWOULDBLOCK;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&listen->waitQueue, &listen->lock, listen->isClosed || !list_is_empty(&listen->backlog)) ==
            ERR)
        {
            return ERR;
        }
    }
    UNREF_DEFER(conn);

    assert(conn != NULL);

    local_socket_t* newData = newSock->data;
    if (newData == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    newData->conn = REF(conn);
    newData->isServer = true;

    return 0;
}

static size_t local_socket_send(socket_t* sock, const void* buffer, size_t count, size_t* offset, mode_t mode)
{
    UNUSED(offset);

    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_conn_t* conn = local_socket_get_conn(data);
    if (conn == NULL)
    {
        errno = ECONNRESET;
        return ERR;
    }
    UNREF_DEFER(conn);
    LOCK_SCOPE(&conn->lock);

    if (conn->isClosed)
    {
        errno = EPIPE;
        return ERR;
    }

    if (count > LOCAL_MAX_PACKET_SIZE)
    {
        errno = EMSGSIZE;
        return ERR;
    }

    fifo_t* ring = data->isServer ? &conn->serverToClient : &conn->clientToServer;

    local_packet_header_t header = {.magic = LOCAL_PACKET_MAGIC, .size = count};

    size_t totalSize = sizeof(local_packet_header_t) + count;
    while (fifo_bytes_writeable(ring) < totalSize)
    {
        if (conn->isClosed)
        {
            errno = EPIPE;
            return ERR;
        }
        if (mode & MODE_NONBLOCK)
        {
            errno = EAGAIN;
            return ERR;
        }
        if (WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock, conn->isClosed || fifo_bytes_writeable(ring) >= totalSize) ==
            ERR)
        {
            return ERR;
        }
        if (conn->isClosed)
        {
            errno = EPIPE;
            return ERR;
        }
    }

    fifo_write(ring, &header, sizeof(local_packet_header_t));
    fifo_write(ring, buffer, count);

    wait_unblock(&conn->waitQueue, WAIT_ALL, EOK);
    return count;
}

static size_t local_socket_recv(socket_t* sock, void* buffer, size_t count, size_t* offset, mode_t mode)
{
    UNUSED(offset);

    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_conn_t* conn = local_socket_get_conn(data);
    if (conn == NULL)
    {
        errno = ECONNRESET;
        return ERR;
    }
    UNREF_DEFER(conn);
    LOCK_SCOPE(&conn->lock);

    fifo_t* ring = data->isServer ? &conn->clientToServer : &conn->serverToClient;

    while (fifo_bytes_readable(ring) < sizeof(local_packet_header_t))
    {
        if (conn->isClosed)
        {
            return 0; // EOF
        }
        if (mode & MODE_NONBLOCK)
        {
            errno = EWOULDBLOCK;
            return ERR;
        }
        if (WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock,
                conn->isClosed || fifo_bytes_readable(ring) >= sizeof(local_packet_header_t)) == ERR)
        {
            return ERR;
        }
    }

    local_packet_header_t header;
    fifo_read(ring, &header, sizeof(local_packet_header_t));

    if (header.magic != LOCAL_PACKET_MAGIC)
    {
        errno = EBADMSG;
        conn->isClosed = true;
        wait_unblock(&conn->waitQueue, WAIT_ALL, EOK);
        return ERR;
    }

    if (header.size > LOCAL_MAX_PACKET_SIZE)
    {
        errno = EMSGSIZE;
        conn->isClosed = true;
        wait_unblock(&conn->waitQueue, WAIT_ALL, EOK);
        return ERR;
    }

    size_t readCount = header.size < count ? header.size : count;
    fifo_read(ring, buffer, readCount);

    if (header.size > readCount)
    {
        uint64_t remaining = header.size - readCount;
        char temp[128];
        while (remaining > 0)
        {
            uint64_t toRead = remaining < sizeof(temp) ? remaining : sizeof(temp);
            fifo_read(ring, temp, toRead);
            remaining -= toRead;
        }
    }
    wait_unblock(&conn->waitQueue, WAIT_ALL, EOK);
    return readCount;
}

static wait_queue_t* local_socket_poll(socket_t* sock, poll_events_t* revents)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    switch (sock->state)
    {
    case SOCKET_LISTENING:
    {
        local_listen_t* listen = data->listen;
        if (listen == NULL)
        {
            *revents |= POLLERR;
            return NULL;
        }

        LOCK_SCOPE(&listen->lock);
        if (listen->isClosed)
        {
            *revents |= POLLERR;
        }
        else if (listen->pendingAmount > 0)
        {
            *revents |= POLLIN;
        }

        return &listen->waitQueue;
    }
    case SOCKET_CONNECTED:
    {
        local_conn_t* conn = data->conn;
        if (conn == NULL)
        {
            *revents |= POLLERR;
            return NULL;
        }

        LOCK_SCOPE(&conn->lock);
        if (conn->isClosed)
        {
            *revents |= POLLHUP;
        }
        else
        {
            fifo_t* readRing = data->isServer ? &conn->clientToServer : &conn->serverToClient;
            fifo_t* writeRing = data->isServer ? &conn->serverToClient : &conn->clientToServer;

            if (fifo_bytes_readable(readRing) >= sizeof(local_packet_header_t))
            {
                *revents |= POLLIN;
            }

            if (fifo_bytes_writeable(writeRing) >= sizeof(local_packet_header_t) + 1)
            {
                *revents |= POLLOUT;
            }
        }

        return &conn->waitQueue;
    }
    default:
        errno = EINVAL;
        return NULL;
    }
}

static netfs_family_t local = {
    .name = "local",
    .init = local_socket_init,
    .deinit = local_socket_deinit,
    .bind = local_socket_bind,
    .listen = local_socket_listen,
    .connect = local_socket_connect,
    .accept = local_socket_accept,
    .send = local_socket_send,
    .recv = local_socket_recv,
    .poll = local_socket_poll,
};

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        if (netfs_family_register(&local) == ERR)
        {
            return ERR;
        }
        break;
    case MODULE_EVENT_UNLOAD:
        netfs_family_unregister(&local);
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Local Networking", "Kai Norberg", "Local networking module", OS_VERSION, "MIT", "BOOT_ALWAYS");