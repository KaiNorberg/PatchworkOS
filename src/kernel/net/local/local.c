#include "local.h"

#include "fs/path.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "net/local/local_conn.h"
#include "net/local/local_listen.h"
#include "net/socket.h"
#include "net/socket_family.h"
#include "net/socket_type.h"
#include "sched/wait.h"
#include "sync/lock.h"
#include "utils/ref.h"
#include "utils/ring.h"

#include <_internal/CONTAINER_OF.h>
#include <_internal/MAX_NAME.h>
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <sys/list.h>

static local_listen_t* local_socket_data_get_listen(local_socket_data_t* data)
{
    LOCK_SCOPE(&data->lock);

    if (data->listen.listen == NULL)
    {
        return NULL;
    }

    return REF(data->listen.listen);
}

static local_conn_t* local_socket_data_get_conn(local_socket_data_t* data)
{
    LOCK_SCOPE(&data->lock);

    if (data->conn.conn == NULL)
    {
        return NULL;
    }

    return REF(data->conn.conn);
}

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

    LOG_INFO("local socket: deinit\n");

    lock_acquire(&data->lock);

    switch (sock->currentState)
    {
    case SOCKET_LISTENING:
        if (data->listen.listen != NULL)
        {
            lock_acquire(&data->listen.listen->lock);
            data->listen.listen->isClosed = true;
            wait_unblock(&data->listen.listen->waitQueue, WAIT_ALL);
            lock_release(&data->listen.listen->lock);

            DEREF(data->listen.listen);
            data->listen.listen = NULL;
        }
        break;
    case SOCKET_CONNECTED:
        if (data->conn.conn != NULL)
        {
            lock_acquire(&data->conn.conn->lock);
            data->conn.conn->isClosed = true;
            wait_unblock(&data->conn.conn->waitQueue, WAIT_ALL);
            lock_release(&data->conn.conn->lock);

            DEREF(data->conn.conn);
            data->conn.conn = NULL;
        }
        break;
    default:
        break;
    }

    lock_release(&data->lock);

    heap_free(data);
    sock->private = NULL;
}

static uint64_t local_socket_bind(socket_t* sock, const char* address)
{
    if (address[0] == '\0')
    {
        errno = EINVAL;
        return ERR;
    }

    local_socket_data_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    LOCK_SCOPE(&data->lock);

    local_listen_t* listen = local_listen_new(address);
    if (listen == NULL)
    {
        return ERR;
    }
    REF_DEFER(listen);

    data->listen.listen = REF(listen);
    return 0;
}

static uint64_t local_socket_listen(socket_t* sock, uint32_t backlog)
{
    if (backlog == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    local_socket_data_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    LOCK_SCOPE(&data->lock);

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

static uint64_t local_socket_connect(socket_t* sock, const char* address)
{
    if (address[0] == '\0')
    {
        errno = EINVAL;
        return ERR;
    }

    local_socket_data_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    LOCK_SCOPE(&data->lock);

    local_listen_t* listen = local_listen_find(address);
    if (listen == NULL)
    {
        errno = ECONNREFUSED;
        return ERR;
    }
    REF_DEFER(listen);

    local_conn_t* conn = local_conn_new(listen);
    if (conn == NULL)
    {
        return ERR;
    }
    REF_DEFER(conn);

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
    list_push(&listen->backlog, &conn->entry);

    wait_unblock(&listen->waitQueue, WAIT_ALL);

    data->conn.conn = REF(conn);
    data->conn.isServer = false;
    return 0;
}

static uint64_t local_socket_accept(socket_t* sock, socket_t* newSock)
{
    local_socket_data_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_listen_t* listen = local_socket_data_get_listen(data);
    if (listen == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    REF_DEFER(listen);

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
            list_entry_t* entry = list_first(&listen->backlog);
            local_conn_t* container = CONTAINER_OF(entry, local_conn_t, entry);
            list_remove(entry);
            conn = REF(container);
            listen->pendingAmount--;
            break;
        }

        if (sock->flags & PATH_NONBLOCK)
        {
            errno = EWOULDBLOCK;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&listen->waitQueue, &listen->lock, listen->isClosed || !list_is_empty(&listen->backlog)) !=
            WAIT_NORM)
        {
            errno = EINTR;
            return ERR;
        }
    }
    REF_DEFER(conn);

    assert(conn != NULL);

    local_socket_data_t* newData = newSock->private;
    if (newData == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    lock_acquire(&newData->lock);
    newData->conn.conn = REF(conn);
    newData->conn.isServer = true;
    lock_release(&newData->lock);

    return 0;
}

static uint64_t local_socket_send(socket_t* sock, const void* buffer, uint64_t count, uint64_t* offset)
{
    local_socket_data_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_conn_t* conn = local_socket_data_get_conn(data);
    if (conn == NULL)
    {
        errno = ECONNRESET;
        return ERR;
    }
    REF_DEFER(conn);
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
    if (ring_free_length(ring) < totalSize)
    {
        if (sock->flags & PATH_NONBLOCK)
        {
            errno = EAGAIN;
            return ERR;
        }

        if (conn->isClosed)
        {
            errno = ECONNRESET;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock, conn->isClosed || ring_free_length(ring) >= totalSize) !=
            WAIT_NORM)
        {
            errno = EINTR;
            return ERR;
        }
    }

    if (ring_write(ring, &header, sizeof(local_packet_header_t)) != sizeof(local_packet_header_t) ||
        ring_write(ring, buffer, count) != count)
    {
        errno = EIO;
        return ERR;
    }

    wait_unblock(&conn->waitQueue, WAIT_ALL);
    return count;
}

static uint64_t local_socket_recv(socket_t* sock, void* buffer, uint64_t count, uint64_t* offset)
{
    local_socket_data_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    local_conn_t* conn = local_socket_data_get_conn(data);
    if (conn == NULL)
    {
        errno = ECONNRESET;
        return ERR;
    }
    REF_DEFER(conn);
    LOCK_SCOPE(&conn->lock);

    ring_t* ring = data->conn.isServer ? &conn->clientToServer : &conn->serverToClient;

    if (conn->isClosed)
    {
        return 0; // EOF
    }

    if (ring_data_length(ring) < sizeof(local_packet_header_t))
    {
        if (sock->flags & PATH_NONBLOCK)
        {
            errno = EWOULDBLOCK;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock,
                conn->isClosed || ring_data_length(ring) >= sizeof(local_packet_header_t)) != WAIT_NORM)
        {
            errno = EINTR;
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
        return ERR;
    }

    if (header.size > LOCAL_MAX_PACKET_SIZE)
    {
        errno = EMSGSIZE;
        return ERR;
    }

    size_t readCount = header.size < count ? header.size : count;

    if (ring_read_at(ring, 0, buffer, readCount) != readCount)
    {
        errno = EIO;
        return ERR;
    }

    // Consume entire packet.
    ring_move_read_forward(ring, header.size);
    wait_unblock(&conn->waitQueue, WAIT_ALL);
    return readCount;
}

static wait_queue_t* local_socket_poll(socket_t* sock, poll_events_t* revents)
{
    local_socket_data_t* data = sock->private;
    if (data == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    LOCK_SCOPE(&data->lock);

    switch (sock->currentState)
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

static socket_family_t family = {
    .name = "local",
    .supportedTypes = SOCKET_SEQPACKET,
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

void net_local_init(void)
{
    if (socket_family_register(&family) == ERR)
    {
        panic(NULL, "Failed to register local socket family");
    }

    local_listen_dir_init(&family);
}
