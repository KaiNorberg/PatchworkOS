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
#include <sys/status.h>

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

static status_t local_socket_init(socket_t* sock)
{
    if (sock->type != SOCKET_SEQPACKET)
    {
        return ERR(PROTO, INVAL);
    }

    local_socket_t* data = calloc(1, sizeof(local_socket_t));
    if (data == NULL)
    {
        return ERR(PROTO, NOMEM);
    }
    sock->data = data;
    return OK;
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
        wait_unblock(&data->listen->waitQueue, WAIT_ALL, OK);
        lock_release(&data->listen->lock);
        UNREF(data->listen);
    }

    if (data->conn != NULL)
    {
        lock_acquire(&data->conn->lock);
        data->conn->isClosed = true;
        wait_unblock(&data->conn->waitQueue, WAIT_ALL, OK);
        lock_release(&data->conn->lock);
        UNREF(data->conn);
    }

    free(data);
    sock->data = NULL;
}

static status_t local_socket_bind(socket_t* sock)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    if (data->listen != NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_listen_t* listen;
    status_t status = local_listen_new(sock->address, &listen);
    if (IS_ERR(status))
    {
        return status;
    }

    data->listen = listen;
    return OK;
}

static status_t local_socket_listen(socket_t* sock, uint32_t backlog)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_listen_t* listen = data->listen;
    if (listen == NULL)
    {
        return ERR(PROTO, INVAL);
    }
    LOCK_SCOPE(&listen->lock);

    if (backlog < LOCAL_MAX_BACKLOG)
    {
        listen->maxBacklog = backlog;
    }

    listen->isClosed = false;
    return OK;
}

static status_t local_socket_connect(socket_t* sock)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    if (data->conn != NULL)
    {
        return ERR(PROTO, ALREADY_INIT);
    }

    local_listen_t* listen;
    status_t status = local_listen_find(sock->address, &listen);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(listen);

    local_conn_t* conn = local_conn_new(listen);
    if (conn == NULL)
    {
        return ERR(PROTO, NOMEM);
    }
    UNREF_DEFER(conn);

    LOCK_SCOPE(&listen->lock);

    if (listen->isClosed)
    {
        return ERR(PROTO, NOENT);
    }

    if (listen->pendingAmount >= listen->maxBacklog)
    {
        return ERR(PROTO, BUSY);
    }

    listen->pendingAmount++;
    list_push_back(&listen->backlog, &conn->entry);

    wait_unblock(&listen->waitQueue, WAIT_ALL, OK);

    data->conn = REF(conn);
    data->isServer = false;
    return OK;
}

static status_t local_socket_accept(socket_t* sock, socket_t* newSock, mode_t mode)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_listen_t* listen = local_socket_get_listen(data);
    if (listen == NULL)
    {
        return ERR(PROTO, INVAL);
    }
    UNREF_DEFER(listen);

    local_conn_t* conn = NULL;
    while (true)
    {
        LOCK_SCOPE(&listen->lock);

        if (listen->isClosed)
        {
            return ERR(PROTO, CANCELLED);
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
            return ERR(PROTO, AGAIN);
        }

        status_t status = WAIT_BLOCK_LOCK(&listen->waitQueue, &listen->lock, listen->isClosed || !list_is_empty(&listen->backlog));
        if (IS_ERR(status))
        {
            return status;
        }
    }
    UNREF_DEFER(conn);

    assert(conn != NULL);

    local_socket_t* newData = newSock->data;
    if (newData == NULL)
    {
        return ERR(PROTO, INVAL);
    }
    newData->conn = REF(conn);
    newData->isServer = true;

    return OK;
}

static status_t local_socket_send(socket_t* sock, const void* buffer, size_t count, size_t* offset, size_t* bytesSent, mode_t mode)
{
    UNUSED(offset);

    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_conn_t* conn = local_socket_get_conn(data);
    if (conn == NULL)
    {
        return ERR(PROTO, NOT_INIT);
    }
    UNREF_DEFER(conn);
    LOCK_SCOPE(&conn->lock);

    if (conn->isClosed)
    {
        return ERR(PROTO, IO);
    }

    if (count > LOCAL_MAX_PACKET_SIZE)
    {
        return ERR(PROTO, TOOBIG);
    }

    fifo_t* ring = data->isServer ? &conn->serverToClient : &conn->clientToServer;

    local_packet_header_t header = {.magic = LOCAL_PACKET_MAGIC, .size = count};

    size_t totalSize = sizeof(local_packet_header_t) + count;
    while (fifo_bytes_writeable(ring) < totalSize)
    {
        if (conn->isClosed)
        {
            return ERR(PROTO, IO);
        }
        if (mode & MODE_NONBLOCK)
        {
            return ERR(PROTO, AGAIN);
        }
        status_t status = WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock, conn->isClosed || fifo_bytes_writeable(ring) >= totalSize);
        if (IS_ERR(status))
        {
            return status;
        }
        if (conn->isClosed)
        {
            return ERR(PROTO, IO);
        }
    }

    fifo_write(ring, &header, sizeof(local_packet_header_t));
    fifo_write(ring, buffer, count);

    wait_unblock(&conn->waitQueue, WAIT_ALL, OK);
    *bytesSent = count;
    return OK;
}

static status_t local_socket_recv(socket_t* sock, void* buffer, size_t count, size_t* offset, size_t* bytesReceived, mode_t mode)
{
    UNUSED(offset);

    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_conn_t* conn = local_socket_get_conn(data);
    if (conn == NULL)
    {
        return ERR(PROTO, NOT_INIT);
    }
    UNREF_DEFER(conn);
    LOCK_SCOPE(&conn->lock);

    fifo_t* ring = data->isServer ? &conn->clientToServer : &conn->serverToClient;

    while (fifo_bytes_readable(ring) < sizeof(local_packet_header_t))
    {
        if (conn->isClosed)
        {
            *bytesReceived = 0;
            return OK; // EOF
        }
        if (mode & MODE_NONBLOCK)
        {
            return ERR(PROTO, AGAIN);
        }
        status_t status = WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock,
                conn->isClosed || fifo_bytes_readable(ring) >= sizeof(local_packet_header_t));
        if (IS_ERR(status))
        {
            return status;
        }
    }

    local_packet_header_t header;
    fifo_read(ring, &header, sizeof(local_packet_header_t));

    if (header.magic != LOCAL_PACKET_MAGIC)
    {
        conn->isClosed = true;
        wait_unblock(&conn->waitQueue, WAIT_ALL, OK);
        return ERR(PROTO, ILSEQ);
    }

    if (header.size > LOCAL_MAX_PACKET_SIZE)
    {
        conn->isClosed = true;
        wait_unblock(&conn->waitQueue, WAIT_ALL, OK);
        return ERR(PROTO, TOOBIG);
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
    wait_unblock(&conn->waitQueue, WAIT_ALL, OK);
    *bytesReceived = readCount;
    return OK;
}

static status_t local_socket_poll(socket_t* sock, poll_events_t* revents, wait_queue_t** queue)
{
    local_socket_t* data = sock->data;
    if (data == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    switch (sock->state)
    {
    case SOCKET_LISTENING:
    {
        local_listen_t* listen = data->listen;
        if (listen == NULL)
        {
            *revents |= POLLERR;
            return OK;
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

        *queue = &listen->waitQueue;
        return OK;
    }
    case SOCKET_CONNECTED:
    {
        local_conn_t* conn = data->conn;
        if (conn == NULL)
        {
            *revents |= POLLERR;
            return OK;
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

        *queue = &conn->waitQueue;
        return OK;
    }
    default:
        return ERR(PROTO, INVAL);
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

status_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
    {
        status_t status = netfs_family_register(&local);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    break;
    case MODULE_EVENT_UNLOAD:
        netfs_family_unregister(&local);
        break;
    default:
        break;
    }

    return OK;
}

MODULE_INFO("Local Networking", "Kai Norberg", "Local networking module", OS_VERSION, "MIT", "BOOT_ALWAYS");