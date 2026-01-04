#include "local.h"

#include "local_conn.h"
#include "local_listen.h"

#include <kernel/fs/filesystem.h>
#include <kernel/fs/path.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/module/module.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ref.h>
#include <kernel/utils/ring.h>
#include <kernel/fs/netfs.h>

#include <stdlib.h>
#include <sys/io.h>
#include <sys/list.h>

static local_listen_t* local_socket_get_listen(local_socket_t* data)
{
    if (data->listen.listen == NULL)
    {
        return NULL;
    }

    return REF(data->listen.listen);
}

static local_conn_t* local_socket_get_conn(local_socket_t* data)
{
    if (data->conn.conn == NULL)
    {
        return NULL;
    }

    return REF(data->conn.conn);
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
    sock->private = data;
    return 0;
}

static void local_socket_deinit(socket_t* sock)
{
    local_socket_t* data = sock->private;
    if (data == NULL)
    {
        return;
    }

    switch (sock->state)
    {
    case SOCKET_LISTENING:
        if (data->listen.listen != NULL)
        {
            lock_acquire(&data->listen.listen->lock);
            data->listen.listen->isClosed = true;
            wait_unblock(&data->listen.listen->waitQueue, WAIT_ALL, EOK);
            lock_release(&data->listen.listen->lock);

            UNREF(data->listen.listen);
            data->listen.listen = NULL;
        }
        break;
    case SOCKET_CONNECTED:
        if (data->conn.conn != NULL)
        {
            lock_acquire(&data->conn.conn->lock);
            data->conn.conn->isClosed = true;
            wait_unblock(&data->conn.conn->waitQueue, WAIT_ALL, EOK);
            lock_release(&data->conn.conn->lock);

            UNREF(data->conn.conn);
            data->conn.conn = NULL;
        }
        break;
    default:
        break;
    }

    free(data);
    sock->private = NULL;
}

static uint64_t local_socket_bind(socket_t* sock)
{
    local_socket_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_listen_t* listen = local_listen_new(sock->address);
    if (listen == NULL)
    {
        return ERR;
    }

    data->listen.listen = listen;
    return 0;
}

static uint64_t local_socket_listen(socket_t* sock, uint32_t backlog)
{
    local_socket_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_listen_t* listen = data->listen.listen;
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
    local_socket_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
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

    data->conn.conn = REF(conn);
    data->conn.isServer = false;
    return 0;
}

static uint64_t local_socket_accept(socket_t* sock, socket_t* newSock, mode_t mode)
{
    local_socket_t* data = sock->private;
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

    local_socket_t* newData = newSock->private;
    if (newData == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    newData->conn.conn = REF(conn);
    newData->conn.isServer = true;

    return 0;
}

static uint64_t local_socket_send(socket_t* sock, const void* buffer, uint64_t count, uint64_t* offset, mode_t mode)
{
    UNUSED(offset);

    local_socket_t* data = sock->private;
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
        errno = ECONNRESET;
        return ERR;
    }

    if (count > LOCAL_MAX_PACKET_SIZE)
    {
        errno = EMSGSIZE;
        return ERR;
    }

    ring_t* ring = data->conn.isServer ? &conn->serverToClient : &conn->clientToServer;

    local_packet_header_t header = {.magic = LOCAL_PACKET_MAGIC, .size = count};

    uint64_t totalSize = sizeof(local_packet_header_t) + count;

    while (ring_free_length(ring) < totalSize)
    {
        if (conn->isClosed)
        {
            errno = ECONNRESET;
            return ERR;
        }
        if (mode & MODE_NONBLOCK)
        {
            errno = EAGAIN;
            return ERR;
        }
        if (WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock, conn->isClosed || ring_free_length(ring) >= totalSize) ==
            ERR)
        {
            return ERR;
        }
        if (conn->isClosed)
        {
            errno = ECONNRESET;
            return ERR;
        }
    }

    if (ring_write(ring, &header, sizeof(local_packet_header_t)) != sizeof(local_packet_header_t) ||
        ring_write(ring, buffer, count) != count)
    {
        errno = EIO;
        return ERR;
    }

    wait_unblock(&conn->waitQueue, WAIT_ALL, EOK);
    return count;
}

static uint64_t local_socket_recv(socket_t* sock, void* buffer, uint64_t count, uint64_t* offset, mode_t mode)
{
    UNUSED(offset);

    local_socket_t* data = sock->private;
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

    ring_t* ring = data->conn.isServer ? &conn->clientToServer : &conn->serverToClient;

    while (ring_data_length(ring) < sizeof(local_packet_header_t))
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
                conn->isClosed || ring_data_length(ring) >= sizeof(local_packet_header_t)) == ERR)
        {
            return ERR;
        }
    }

    local_packet_header_t header;
    if (ring_read(ring, &header, sizeof(local_packet_header_t)) != sizeof(local_packet_header_t))
    {
        errno = EIO;
        return ERR;
    }

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

    uint64_t readCount = header.size < count ? header.size : count;
    if (ring_read_at(ring, 0, buffer, readCount) != readCount)
    {
        LOG_DEBUG("failed to read local socket packet data\n");
        errno = EIO;
        return ERR;
    }

    // Consume entire packet.
    ring_move_read_forward(ring, header.size);
    wait_unblock(&conn->waitQueue, WAIT_ALL, EOK);
    return readCount;
}

static wait_queue_t* local_socket_poll(socket_t* sock, poll_events_t* revents)
{
    local_socket_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    switch (sock->state)
    {
    case SOCKET_LISTENING:
    {
        local_listen_t* listen = data->listen.listen;
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
        local_conn_t* conn = data->conn.conn;
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
            ring_t* readRing = data->conn.isServer ? &conn->clientToServer : &conn->serverToClient;
            ring_t* writeRing = data->conn.isServer ? &conn->serverToClient : &conn->clientToServer;

            if (ring_data_length(readRing) >= sizeof(local_packet_header_t))
            {
                *revents |= POLLIN;
            }

            if (ring_free_length(writeRing) >= sizeof(local_packet_header_t) + 1)
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