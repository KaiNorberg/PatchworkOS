#include "local.h"

#include "fs/sysfs.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "sched/thread.h"
#include "sched/wait.h"
#include "socket.h"
#include "sync/lock.h"
#include "sys/io.h"
#include "utils/ring.h"

#include <assert.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/math.h>

static list_t listeners;
static lock_t listenersLock;

static local_connection_t* local_connection_ref(local_connection_t* conn);
static void local_connection_deref(local_connection_t* conn);

static local_connection_t* local_listener_pop(local_listener_t* listener);

static file_ops_t listenerOps = {
    // None
};

static local_listener_t* local_listener_new(const char* address)
{
    if (!vfs_is_name_valid(address))
    {
        errno = EINVAL;
        return NULL;
    }

    local_listener_t* listener = heap_alloc(sizeof(local_listener_t), HEAP_NONE);
    if (listener == NULL)
    {
        return NULL;
    }

    list_entry_init(&listener->entry);
    strncpy(listener->address, address, MAX_NAME - 1);
    listener->address[MAX_NAME - 1] = '\0';
    listener->backlog.readIndex = 0;
    listener->backlog.writeIndex = 0;
    listener->backlog.count = 0;
    lock_init(&listener->lock);
    atomic_store(&listener->ref, 1);

    if (sysfile_init_path(&listener->sysfile, "/net/local/listen", address, &listenerOps, NULL) != 0)
    {
        heap_free(listener);
        return NULL;
    }

    wait_queue_init(&listener->waitQueue);

    lock_acquire(&listenersLock);
    list_push(&listeners, &listener->entry);
    lock_release(&listenersLock);
    return listener;
}

static local_listener_t* local_listener_ref(local_listener_t* listener)
{
    atomic_fetch_add(&listener->ref, 1);
    return listener;
}

static void local_listener_deref(local_listener_t* listener)
{
    if (atomic_fetch_sub(&listener->ref, 1) <= 1)
    {
        lock_acquire(&listenersLock);
        list_remove(&listener->entry);
        lock_release(&listenersLock);

        wait_unblock(&listener->waitQueue, UINT64_MAX);

        lock_acquire(&listener->lock);
        while (listener->backlog.count > 0)
        {
            local_connection_t* conn = local_listener_pop(listener);
            if (conn != NULL)
            {
                wait_unblock(&conn->waitQueue, UINT64_MAX);
                local_connection_deref(conn);
            }
        }
        lock_release(&listener->lock);

        wait_queue_deinit(&listener->waitQueue);
        sysfile_deinit(&listener->sysfile, NULL);
        heap_free(listener);
    }
}

static local_listener_t* local_listener_find(const char* address)
{
    LOCK_DEFER(&listenersLock);

    local_listener_t* listener;
    LIST_FOR_EACH(listener, &listeners, entry)
    {
        if (strcmp(address, listener->address) == 0)
        {
            return local_listener_ref(listener);
        }
    }

    return NULL;
}

static bool local_listener_is_conn_avail(local_listener_t* listener)
{
    return listener->backlog.count > 0;
}

static bool local_listener_is_closed(local_listener_t* listener)
{
    return atomic_load(&listener->ref) == 1;
}

static bool listener_can_accept(local_listener_t* listener)
{
    return listener->backlog.count < LOCAL_BACKLOG_MAX;
}

static void local_listener_push(local_listener_t* listener, local_connection_t* conn)
{
    listener->backlog.buffer[listener->backlog.writeIndex] = local_connection_ref(conn);
    listener->backlog.writeIndex = (listener->backlog.writeIndex + 1) % LOCAL_BACKLOG_MAX;
    listener->backlog.count++;
}

static local_connection_t* local_listener_pop(local_listener_t* listener)
{
    local_connection_t* conn = listener->backlog.buffer[listener->backlog.readIndex];
    listener->backlog.readIndex = (listener->backlog.readIndex + 1) % LOCAL_BACKLOG_MAX;
    listener->backlog.count--;
    return conn; // Transfer reference
}

static local_connection_t* local_connection_new(const char* address)
{
    local_connection_t* conn = heap_alloc(sizeof(local_connection_t), HEAP_NONE);
    if (conn == NULL)
    {
        return NULL;
    }

    void* serverBuffer = heap_alloc(LOCAL_BUFFER_SIZE, HEAP_VMM);
    if (serverBuffer == NULL)
    {
        heap_free(conn);
        return NULL;
    }
    ring_init(&conn->serverToClient, serverBuffer, LOCAL_BUFFER_SIZE);

    void* clientBuffer = heap_alloc(LOCAL_BUFFER_SIZE, HEAP_VMM);
    if (clientBuffer == NULL)
    {
        heap_free(serverBuffer);
        heap_free(conn);
        return NULL;
    }
    ring_init(&conn->clientToServer, clientBuffer, LOCAL_BUFFER_SIZE);

    conn->listener = local_listener_find(address);
    if (conn->listener == NULL)
    {
        heap_free(serverBuffer);
        heap_free(clientBuffer);
        heap_free(conn);
        return NULL;
    }

    lock_init(&conn->lock);
    wait_queue_init(&conn->waitQueue);
    atomic_init(&conn->ref, 1);
    atomic_init(&conn->isAccepted, false);

    return conn;
}

static local_connection_t* local_connection_ref(local_connection_t* conn)
{
    atomic_fetch_add(&conn->ref, 1);
    return conn;
}

static void local_connection_deref(local_connection_t* conn)
{
    if (atomic_fetch_sub(&conn->ref, 1) <= 1)
    {
        local_listener_deref(conn->listener);
        wait_unblock(&conn->waitQueue, UINT64_MAX);
        wait_queue_deinit(&conn->waitQueue);
        heap_free(conn->serverToClient.buffer);
        heap_free(conn->clientToServer.buffer);
        heap_free(conn);
    }
}

static bool local_connection_is_closed(local_connection_t* conn)
{
    return atomic_load(&conn->ref) == 1 || atomic_load(&conn->listener->ref) == 1;
}

static ring_t* local_socket_get_send_ring(local_socket_t* local)
{
    switch (local->state)
    {
    case LOCAL_SOCKET_CONNECT:
        return &local->connect.conn->clientToServer;
    case LOCAL_SOCKET_ACCEPT:
        return &local->accept.conn->serverToClient;
    default:
        return NULL;
    }
}

static ring_t* local_socket_get_receive_ring(local_socket_t* local)
{
    switch (local->state)
    {
    case LOCAL_SOCKET_CONNECT:
        return &local->connect.conn->serverToClient;
    case LOCAL_SOCKET_ACCEPT:
        return &local->accept.conn->clientToServer;
    default:
        return NULL;
    }
}

static local_connection_t* local_socket_get_conn(local_socket_t* local)
{
    switch (local->state)
    {
    case LOCAL_SOCKET_CONNECT:
        return local->connect.conn;
    case LOCAL_SOCKET_ACCEPT:
        return local->accept.conn;
    default:
        return NULL;
    }
}

static uint64_t local_socket_init(socket_t* socket)
{
    local_socket_t* local = heap_alloc(sizeof(local_socket_t), HEAP_NONE);
    if (local == NULL)
    {
        return ERR;
    }
    local->state = LOCAL_SOCKET_BLANK;
    lock_init(&local->lock);

    socket->private = local;
    return 0;
}

static void local_socket_deinit(socket_t* socket)
{
    local_socket_t* local = socket->private;
    switch (local->state)
    {
    case LOCAL_SOCKET_BLANK:
    case LOCAL_SOCKET_BOUND:
        break;
    case LOCAL_SOCKET_LISTEN:
        local_listener_deref(local->listen.listener);
        break;
    case LOCAL_SOCKET_CONNECT:
        wait_unblock(&local->connect.conn->waitQueue, UINT64_MAX);
        local_connection_deref(local->connect.conn);
        break;
    case LOCAL_SOCKET_ACCEPT:
        wait_unblock(&local->accept.conn->waitQueue, UINT64_MAX);
        local_connection_deref(local->accept.conn);
        break;
    }

    heap_free(local);
}

static uint64_t local_socket_bind(socket_t* socket, const char* address)
{
    local_socket_t* local = socket->private;
    LOCK_DEFER(&local->lock);

    if (local->state != LOCAL_SOCKET_BLANK)
    {
        errno = ENOTSUP;
        return ERR;
    }
    if (!vfs_is_name_valid(address))
    {
        errno = EINVAL;
        return ERR;
    }

    strncpy(local->bind.address, address, MAX_NAME - 1);
    local->bind.address[MAX_NAME - 1] = '\0';
    local->state = LOCAL_SOCKET_BOUND;
    return 0;
}

static uint64_t local_socket_listen(socket_t* socket)
{
    local_socket_t* local = socket->private;
    LOCK_DEFER(&local->lock);

    if (local->state != LOCAL_SOCKET_BOUND)
    {
        errno = ENOTSUP;
        return ERR;
    }

    LOG_INFO("%s\n", local->bind.address);
    local_listener_t* listener = local_listener_new(local->bind.address);
    if (listener == NULL)
    {
        return ERR;
    }
    local->listen.listener = listener;
    local->state = LOCAL_SOCKET_LISTEN;
    return 0;
}

static uint64_t local_socket_connect(socket_t* socket, const char* address)
{
    if (!vfs_is_name_valid(address))
    {
        errno = EINVAL;
        return ERR;
    }

    local_socket_t* local = socket->private;
    lock_acquire(&local->lock);
    if (local->state != LOCAL_SOCKET_BLANK)
    {
        lock_release(&local->lock);
        errno = ENOTSUP;
        return ERR;
    }

    local_connection_t* conn = local_connection_new(address); // Reference from here
    if (conn == NULL)
    {
        lock_release(&local->lock);
        return ERR;
    }
    local_listener_t* listener = conn->listener;

    lock_acquire(&listener->lock);
    if (!listener_can_accept(listener))
    {
        lock_release(&local->lock);
        lock_release(&listener->lock);
        local_connection_deref(conn);
        errno = ECONNREFUSED;
        return ERR;
    }

    local_listener_push(listener, conn);
    wait_unblock(&listener->waitQueue, UINT64_MAX);
    lock_release(&listener->lock);

    local->connect.conn = conn; // Reference ends up here
    local->state = LOCAL_SOCKET_CONNECT;
    lock_release(&local->lock);

    if (socket->flags & PATH_NONBLOCK)
    {
        bool isAccepted = atomic_load(&conn->isAccepted);
        bool isClosed = local_connection_is_closed(conn);

        if (!isAccepted && !isClosed)
        {
            errno = EINPROGRESS;
            return ERR;
        }
        if (isClosed)
        {
            errno = ECONNREFUSED;
            return ERR;
        }
    }
    else
    {
        if (WAIT_BLOCK(&conn->waitQueue, atomic_load(&conn->isAccepted) || local_connection_is_closed(conn)) !=
            WAIT_NORM)
        {
            errno = EINTR;
            return ERR;
        }
    }

    if (local_connection_is_closed(conn))
    {
        errno = ECONNREFUSED;
        return ERR;
    }

    return 0;
}

static uint64_t local_socket_accept(socket_t* socket, socket_t* newSocket)
{
    local_socket_t* local = socket->private;

    lock_acquire(&local->lock);
    if (local->state != LOCAL_SOCKET_LISTEN)
    {
        lock_release(&local->lock);
        errno = EINVAL;
        return ERR;
    }
    local_listener_t* listener = local->listen.listener;
    lock_release(&local->lock);

    lock_acquire(&listener->lock);

    if (socket->flags & PATH_NONBLOCK)
    {
        if (!(local_listener_is_conn_avail(listener) || local_listener_is_closed(listener)))
        {
            lock_release(&listener->lock);
            errno = EWOULDBLOCK;
            return ERR;
        }
    }
    else
    {
        if (WAIT_BLOCK_LOCK(&listener->waitQueue, &listener->lock,
                local_listener_is_conn_avail(listener) || local_listener_is_closed(listener)) != WAIT_NORM)
        {
            lock_release(&listener->lock);
            errno = EINTR;
            return ERR;
        }
    }

    if (local_listener_is_closed(listener))
    {
        lock_release(&listener->lock);
        errno = EINVAL;
        return ERR;
    }

    local_socket_t* newLocal = heap_alloc(sizeof(local_socket_t), HEAP_NONE);
    if (newLocal == NULL)
    {
        lock_release(&listener->lock);
        errno = ENOMEM;
        return ERR;
    }
    newLocal->state = LOCAL_SOCKET_ACCEPT;
    lock_init(&newLocal->lock);

    local_connection_t* conn = local_listener_pop(listener);
    if (conn == NULL)
    {
        heap_free(newLocal);
        lock_release(&listener->lock);
        errno = EAGAIN;
        return ERR;
    }

    atomic_store(&conn->isAccepted, true);
    newLocal->accept.conn = conn;
    newSocket->private = newLocal;

    wait_unblock(&conn->waitQueue, UINT64_MAX);
    lock_release(&listener->lock);
    return 0;
}

static uint64_t local_socket_send(socket_t* socket, const void* buffer, uint64_t count, uint64_t* offset)
{
    if (buffer == NULL)
    {
        errno = EFAULT;
        return ERR;
    }
    if (count == 0)
    {
        return 0;
    }
    if (count > LOCAL_MAX_PACKET_SIZE)
    {
        errno = EMSGSIZE;
        return ERR;
    }

    local_socket_t* local = socket->private;
    if (local->state != LOCAL_SOCKET_CONNECT && local->state != LOCAL_SOCKET_ACCEPT)
    {
        errno = ENOTCONN;
        return ERR;
    }

    ring_t* ring = local_socket_get_send_ring(local);
    local_connection_t* conn = local_socket_get_conn(local);

    if (ring == NULL || conn == NULL)
    {
        errno = ENOTCONN;
        return ERR;
    }

    uint64_t requiredLength = count + sizeof(local_packet_header_t);

    lock_acquire(&conn->lock);

    if (socket->flags & PATH_NONBLOCK)
    {
        if (!(ring_free_length(ring) >= requiredLength || local_connection_is_closed(conn)))
        {
            lock_release(&conn->lock);
            errno = EWOULDBLOCK;
            return ERR;
        }
    }
    else
    {
        if (WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock,
                ring_free_length(ring) >= requiredLength || local_connection_is_closed(conn)) != WAIT_NORM)
        {
            lock_release(&conn->lock);
            errno = EINTR;
            return ERR;
        }
    }

    if (local_connection_is_closed(conn))
    {
        lock_release(&conn->lock);
        errno = EPIPE;
        return ERR;
    }

    local_packet_header_t header = {.size = count};
    ring_write(ring, &header, sizeof(local_packet_header_t));
    ring_write(ring, buffer, count);

    wait_unblock(&conn->waitQueue, UINT64_MAX);
    lock_release(&conn->lock);
    return count;
}

static uint64_t local_socket_receive(socket_t* socket, void* buffer, uint64_t count, uint64_t* offset)
{
    if (buffer == NULL)
    {
        errno = EFAULT;
        return ERR;
    }
    if (count == 0)
    {
        return 0;
    }

    local_socket_t* local = socket->private;
    if (local->state != LOCAL_SOCKET_CONNECT && local->state != LOCAL_SOCKET_ACCEPT)
    {
        errno = ENOTCONN;
        return ERR;
    }

    ring_t* ring = local_socket_get_receive_ring(local);
    local_connection_t* conn = local_socket_get_conn(local);

    if (ring == NULL || conn == NULL)
    {
        errno = ENOTCONN;
        return ERR;
    }

    lock_acquire(&conn->lock);

    if (socket->flags & PATH_NONBLOCK)
    {
        if (!(ring_data_length(ring) >= sizeof(local_packet_header_t) || local_connection_is_closed(conn)))
        {
            lock_release(&conn->lock);
            errno = EWOULDBLOCK;
            return ERR;
        }
    }
    else
    {
        if (WAIT_BLOCK_LOCK(&conn->waitQueue, &conn->lock,
                ring_data_length(ring) >= sizeof(local_packet_header_t) || local_connection_is_closed(conn)) !=
            WAIT_NORM)
        {
            lock_release(&conn->lock);
            errno = EINTR;
            return ERR;
        }
    }

    if (local_connection_is_closed(conn))
    {
        lock_release(&conn->lock);
        return 0;
    }

    local_packet_header_t header;
    ring_read_at(ring, 0, &header, sizeof(local_packet_header_t));

    if (header.size > count)
    {
        ring_move_read_forward(ring, sizeof(local_packet_header_t) + header.size);
        wait_unblock(&conn->waitQueue, UINT64_MAX);
        lock_release(&conn->lock);
        errno = EMSGSIZE;
        return ERR;
    }

    uint64_t readCount = header.size;
    if (readCount > 0)
    {
        ring_read_at(ring, sizeof(local_packet_header_t), buffer, readCount);
    }

    ring_move_read_forward(ring, sizeof(local_packet_header_t) + header.size);

    wait_unblock(&conn->waitQueue, UINT64_MAX);
    lock_release(&conn->lock);
    return readCount;
}

static wait_queue_t* local_socket_poll(socket_t* socket, poll_file_t* poll)
{
    local_socket_t* local = socket->private;
    LOCK_DEFER(&local->lock);

    switch (local->state)
    {
    case LOCAL_SOCKET_LISTEN:
    {
        local_listener_t* listener = local->listen.listener;
        LOCK_DEFER(&listener->lock);

        if (local_listener_is_closed(listener))
        {
            poll->occoured = POLL_ERR;
        }
        else
        {
            poll->occoured = local_listener_is_conn_avail(listener) ? POLL_READ : 0;
        }
        return &listener->waitQueue;
    }
    break;
    case LOCAL_SOCKET_CONNECT:
    {
        local_connection_t* conn = local->connect.conn;
        LOCK_DEFER(&conn->lock);
        if (local_connection_is_closed(conn))
        {
            poll->occoured = POLL_READ | POLL_ERR | POLL_HANGUP;
        }
        else
        {
            uint32_t events = 0;
            if (ring_data_length(&conn->serverToClient) >= sizeof(local_packet_header_t))
            {
                events |= POLL_READ;
            }
            if (ring_free_length(&conn->clientToServer) >= sizeof(local_packet_header_t))
            {
                events |= POLL_WRITE;
            }
            poll->occoured = events;
        }
        return &conn->waitQueue;
    }
    break;
    case LOCAL_SOCKET_ACCEPT:
    {
        local_connection_t* conn = local->accept.conn;
        LOCK_DEFER(&conn->lock);
        if (local_connection_is_closed(conn))
        {
            poll->occoured = POLL_READ | POLL_ERR | POLL_HANGUP;
        }
        else
        {
            uint32_t events = 0;
            if (ring_data_length(&conn->clientToServer) >= sizeof(local_packet_header_t))
            {
                events |= POLL_READ;
            }
            if (ring_free_length(&conn->serverToClient) >= sizeof(local_packet_header_t))
            {
                events |= POLL_WRITE;
            }
            poll->occoured = events;
        }
        return &conn->waitQueue;
    }
    break;
    default:
    {
        poll->occoured = POLL_ERR;
        errno = ENOTSUP;
        return NULL;
    }
    }
}

static socket_family_t family = {
    .name = "local",
    .init = local_socket_init,
    .deinit = local_socket_deinit,
    .bind = local_socket_bind,
    .listen = local_socket_listen,
    .accept = local_socket_accept,
    .connect = local_socket_connect,
    .send = local_socket_send,
    .receive = local_socket_receive,
    .poll = local_socket_poll,
};

void net_local_init(void)
{
    list_init(&listeners);
    lock_init(&listenersLock);

    assert(socket_family_register(&family) != ERR);
}
