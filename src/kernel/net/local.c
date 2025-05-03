#include "local.h"

#include "lock.h"
#include "log.h"
#include "path.h"
#include "pmm.h"
#include "ring.h"
#include "sched.h"
#include "socket.h"
#include "sys/io.h"
#include "sysfs.h"
#include "vfs.h"
#include "waitsys.h"

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/math.h>

static list_t listeners;
static lock_t listenersLock;

static local_connection_t* local_connection_ref(local_connection_t* conn);

SYSFS_STANDARD_SYSOBJ_OPS_DEFINE(listenerOps, (file_ops_t){});

static local_listener_t* local_listener_new(const char* address)
{
    if (!path_valid_name(address))
    {
        return NULL;
    }

    local_listener_t* listener = malloc(sizeof(local_listener_t));
    if (listener == NULL)
    {
        return NULL;
    }
    list_entry_init(&listener->entry);
    strcpy(listener->address, address);
    listener->readIndex = 0;
    listener->writeIndex = 0;
    listener->length = 0;
    lock_init(&listener->lock);
    wait_queue_init(&listener->waitQueue);
    atomic_store(&listener->ref, 1);
    listener->obj = sysobj_new("/net/local/listen", address, &listenerOps, NULL);
    if (listener->obj == NULL)
    {
        free(listener);
        return NULL;
    }

    LOCK_DEFER(&listenersLock);
    list_push(&listeners, &listener->entry);
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
        LOCK_DEFER(&listenersLock);
        waitsys_unblock(&listener->waitQueue, UINT64_MAX);
        list_remove(&listener->entry);
        sysobj_free(listener->obj);
        free(listener);
    }
}

static local_listener_t* local_listener_get(const char* address)
{
    if (!path_valid_name(address))
    {
        return NULL;
    }

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

static bool local_listener_conn_avail(local_listener_t* listener)
{
    return listener->length != 0;
}

static bool local_listener_closed(local_listener_t* listener)
{
    return atomic_load(&listener->ref) == 1;
}

static void local_listener_push(local_listener_t* listener, local_connection_t* conn)
{
    listener->backlog[listener->writeIndex] = local_connection_ref(conn);
    listener->writeIndex = (listener->writeIndex + 1) % LOCAL_BACKLOG_MAX;
    listener->length++;
}

static local_connection_t* local_listener_pop(local_listener_t* listener)
{
    local_connection_t* conn = listener->backlog[listener->readIndex];
    listener->readIndex = (listener->readIndex + 1) % LOCAL_BACKLOG_MAX;
    listener->length--;
    return conn; // Transfer reference
}

static local_connection_t* local_connection_new(const char* address)
{
    local_connection_t* conn = malloc(sizeof(local_connection_t));
    if (conn == NULL)
    {
        return NULL;
    }

    void* serverBuffer = malloc(LOCAL_BUFFER_SIZE);
    if (serverBuffer == NULL)
    {
        free(conn);
        return NULL;
    }
    ring_init(&conn->serverRing, serverBuffer, LOCAL_BUFFER_SIZE);
    void* clientBuffer = malloc(LOCAL_BUFFER_SIZE);
    if (clientBuffer == NULL)
    {
        free(serverBuffer);
        free(conn);
        return NULL;
    }
    ring_init(&conn->clientRing, clientBuffer, LOCAL_BUFFER_SIZE);

    conn->listener = local_listener_get(address);
    if (conn->listener == NULL)
    {
        free(serverBuffer);
        free(clientBuffer);
        free(conn);
        return NULL;
    }
    lock_init(&conn->lock);
    wait_queue_init(&conn->waitQueue);
    atomic_init(&conn->ref, 1);
    atomic_init(&conn->accepted, false);

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
        free(conn->serverRing.buffer);
        free(conn->clientRing.buffer);
        local_listener_deref(conn->listener);
        wait_queue_deinit(&conn->waitQueue);
        free(conn);
    }
}

static bool local_connection_closed(local_connection_t* conn)
{
    return atomic_load(&conn->ref) == 1 || atomic_load(&conn->listener->ref) == 1;
}

static uint64_t local_socket_init(socket_t* socket)
{
    local_socket_t* local = malloc(sizeof(local_socket_t));
    if (local == NULL)
    {
        return ERR;
    }
    local->state = LOCAL_SOCKET_BLANK;
    lock_init(&local->lock);

    socket->private = local;
    return 0;
}

static uint64_t local_socket_accept(socket_t* socket, socket_t* newSocket)
{
    local_socket_t* local = socket->private;
    lock_acquire(&local->lock);
    if (local->state != LOCAL_SOCKET_LISTEN)
    {
        lock_release(&local->lock);
        return ERROR(ENOOP);
    }
    local_listener_t* listener = local->listen.listener;
    lock_release(&local->lock);

    if (WAITSYS_BLOCK_LOCK(&listener->waitQueue, &listener->lock,
            local_listener_conn_avail(listener) || local_listener_closed(listener)) != BLOCK_NORM)
    {
        lock_release(&listener->lock);
        return 0;
    }

    if (local_listener_closed(listener))
    {
        lock_release(&listener->lock);
        return ERROR(EINVAL);
    }

    local_socket_t* newLocal = malloc(sizeof(local_socket_t));
    if (newLocal == NULL)
    {
        lock_release(&listener->lock);
        return ERR;
    }
    newLocal->state = LOCAL_SOCKET_ACCEPT;
    lock_init(&newLocal->lock);

    local_connection_t* conn = local_listener_pop(listener);
    atomic_store(&conn->accepted, true);
    newLocal->accept.conn = conn;
    newSocket->private = newLocal;

    lock_release(&listener->lock);
    waitsys_unblock(&conn->waitQueue, UINT64_MAX);
    return 0;
}

static void local_socket_deinit(socket_t* socket)
{
    local_socket_t* local = socket->private;
    switch (local->state)
    {
    case LOCAL_SOCKET_BLANK:
    case LOCAL_SOCKET_BOUND:
    {
        free(local);
    }
    break;
    case LOCAL_SOCKET_LISTEN:
    {
        local_listener_deref(local->listen.listener);
        free(local);
    }
    break;
    case LOCAL_SOCKET_CONNECT:
    {
        local_connection_deref(local->connect.conn);
        waitsys_unblock(&local->connect.conn->waitQueue, UINT64_MAX);
        free(local);
    }
    break;
    case LOCAL_SOCKET_ACCEPT:
    {
        local_connection_deref(local->accept.conn);
        waitsys_unblock(&local->accept.conn->waitQueue, UINT64_MAX);
        free(local);
    }
    break;
    }
}

static uint64_t local_socket_bind(socket_t* socket, const char* address)
{
    local_socket_t* local = socket->private;
    LOCK_DEFER(&local->lock);

    if (local->state != LOCAL_SOCKET_BLANK)
    {
        return ERROR(ENOOP);
    }
    if (!path_valid_name(address))
    {
        return ERROR(EINVAL);
    }

    strcpy(local->bind.address, address);
    local->state = LOCAL_SOCKET_BOUND;
    return 0;
}

static uint64_t local_socket_listen(socket_t* socket)
{
    local_socket_t* local = socket->private;
    LOCK_DEFER(&local->lock);

    if (local->state != LOCAL_SOCKET_BOUND)
    {
        return ERROR(ENOOP);
    }
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
    if (!path_valid_name(address))
    {
        return ERROR(EINVAL);
    }

    local_socket_t* local = socket->private;
    lock_acquire(&local->lock);
    if (local->state != LOCAL_SOCKET_BLANK)
    {
        lock_release(&local->lock);
        return ERROR(ENOOP);
    }

    local_connection_t* conn = local_connection_new(address); // Reference from here
    if (conn == NULL)
    {
        lock_release(&local->lock);
        return ERR;
    }

    local_listener_t* listener = conn->listener;
    lock_acquire(&listener->lock);
    if (listener->length == LOCAL_BACKLOG_MAX)
    {
        lock_release(&local->lock);
        lock_release(&listener->lock);
        local_connection_deref(conn);
        return ERROR(EBUSY);
    }
    local_listener_push(listener, conn);
    waitsys_unblock(&listener->waitQueue, UINT64_MAX);
    lock_release(&listener->lock);

    local->connect.conn = conn; // Refernce ends up here
    local->state = LOCAL_SOCKET_CONNECT;
    lock_release(&local->lock);

    if (WAITSYS_BLOCK(&conn->waitQueue, atomic_load(&conn->accepted) || local_connection_closed(conn)) != BLOCK_NORM)
    {
        return ERR;
    }
    if (local_connection_closed(conn))
    {
        return ERROR(EPIPE);
    }
    return 0;
}

static uint64_t local_socket_get_ring_and_conn(local_socket_t* local, bool sending, ring_t** ring, local_connection_t** conn)
{
    LOCK_DEFER(&local->lock);

    switch (local->state)
    {
    case LOCAL_SOCKET_CONNECT:
    {
        if (sending)
        {
            (*ring) = &local->connect.conn->serverRing;
        }
        else
        {
            (*ring) = &local->connect.conn->clientRing;
        }
        (*conn) = local->accept.conn;
    }
    break;
    case LOCAL_SOCKET_ACCEPT:
    {
        if (sending)
        {
            (*ring) = &local->accept.conn->clientRing;
        }
        else
        {
            (*ring) = &local->accept.conn->serverRing;
        }
        (*conn) = local->accept.conn;
    }
    break;
    default:
    {
        return ERROR(ENOOP);
    }
    }
    return 0;
}

static uint64_t local_socket_send(socket_t* socket, const void* buffer, uint64_t count)
{
    printf("local_socket_send: test1");
    local_socket_t* local = socket->private;
    if (local->state != LOCAL_SOCKET_CONNECT && local->state != LOCAL_SOCKET_ACCEPT)
    {
        return ERROR(ENOOP);
    }
    if (count == 0 || count >= LOCAL_BUFFER_SIZE / 2)
    {
        return ERROR(EINVAL);
    }

    printf("local_socket_send: test2");
    ring_t* ring;
    local_connection_t* conn;
    if (local_socket_get_ring_and_conn(local, true, &ring, &conn) == ERR)
    {
        return ERR;
    }

    printf("local_socket_send: test3");
    if (WAITSYS_BLOCK_LOCK(&conn->waitQueue, &conn->lock,
            ring_free_length(ring) >= count + sizeof(local_packet_header_t) || local_connection_closed(conn)) != BLOCK_NORM)
    {
        lock_release(&conn->lock);
        return 0;
    }
    printf("local_socket_send: test4");


    if (local_connection_closed(conn))
    {
        lock_release(&conn->lock);
        return 0;
    }
    printf("local_socket_send: test5");

    local_packet_header_t header = {.size = count};
    ring_write(ring, &header, sizeof(local_packet_header_t));
    ring_write(ring, buffer, count);
    printf("local_socket_send: test6");

    lock_release(&conn->lock);
    waitsys_unblock(&conn->waitQueue, UINT64_MAX);
    printf("local_socket_send: test7");
    return count;
}

static uint64_t local_socket_receive(socket_t* socket, void* buffer, uint64_t count, uint64_t* offset)
{
    local_socket_t* local = socket->private;
    if (local->state != LOCAL_SOCKET_CONNECT && local->state != LOCAL_SOCKET_ACCEPT)
    {
        return ERROR(ENOOP);
    }
    if (count == 0 || count >= LOCAL_BUFFER_SIZE / 2)
    {
        return ERROR(EINVAL);
    }

    ring_t* ring;
    local_connection_t* conn;
    if (local_socket_get_ring_and_conn(local, false, &ring, &conn) == ERR)
    {
        return ERR;
    }

    if (WAITSYS_BLOCK_LOCK(&conn->waitQueue, &conn->lock,
            ring_data_length(ring) >= sizeof(local_packet_header_t) || local_connection_closed(conn)) != BLOCK_NORM)
    {
        lock_release(&conn->lock);
        return 0;
    }

    if (local_connection_closed(conn))
    {
        lock_release(&conn->lock);
        return 0;
    }

    local_packet_header_t header;
    ring_read_at(ring, 0, &header, sizeof(local_packet_header_t));

    uint64_t readCount = (*offset <= header.size) ? MIN(count, header.size - *offset) : 0;
    if (readCount != 0)
    {
        ring_read_at(ring, sizeof(local_packet_header_t) + *offset, buffer, readCount);
    }

    if (sizeof(local_packet_header_t) + *offset + count >= readCount)
    {
        ring_move_read_forward(ring, sizeof(local_packet_header_t) + header.size);
        *offset = 0;
    }

    lock_release(&conn->lock);
    waitsys_unblock(&conn->waitQueue, UINT64_MAX);
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
        if (local_listener_closed(listener))
        {
            poll->occurred = POLL_READ | POLL_WRITE;
        }
        else
        {
            poll->occurred = (listener->length != 0) ? POLL_READ : 0;
        }
        return &listener->waitQueue;
    }
    break;
    case LOCAL_SOCKET_CONNECT:
    {
        local_connection_t* conn = local->connect.conn;
        LOCK_DEFER(&conn->lock);
        if (local_connection_closed(conn))
        {
            poll->occurred = POLL_READ | POLL_WRITE;
        }
        else
        {
            poll->occurred = 0;
        }
        return &conn->waitQueue;
    }
    break;
    case LOCAL_SOCKET_ACCEPT:
    {
        local_connection_t* conn = local->accept.conn;
        LOCK_DEFER(&conn->lock);
        if (local_connection_closed(conn))
        {
            poll->occurred = POLL_READ | POLL_WRITE;
        }
        else
        {
            poll->occurred = (ring_data_length(&conn->serverRing) != 0 ? POLL_READ : 0) |
                (ring_free_length(&conn->clientRing) >= sizeof(local_packet_header_t) ? POLL_WRITE : 0);
        }
        return &conn->waitQueue;
    }
    break;
    default:
    {
        return ERRPTR(ENOOP);
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

    ASSERT_PANIC(socket_family_expose(&family) != NULL);
}
