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

// TODO: This is incomprehensible fix that.

static local_connection_t* local_connection_new(const char* address)
{
    local_connection_t* conn = malloc(sizeof(local_connection_t));
    if (conn == NULL)
    {
        return NULL;
    }

    void* serverBuffer = pmm_alloc();
    if (serverBuffer == NULL)
    {
        free(conn);
        return NULL;
    }
    ring_init(&conn->serverRing, serverBuffer, PAGE_SIZE);
    void* clientBuffer = pmm_alloc();
    if (clientBuffer == NULL)
    {
        pmm_free(serverBuffer);
        free(conn);
        return NULL;
    }
    ring_init(&conn->clientRing, clientBuffer, PAGE_SIZE);

    char path[MAX_PATH];
    sprintf(path, "sys:/net/local/listen/%s", address);
    conn->listener = vfs_open(path);
    if (conn->listener == NULL)
    {
        pmm_free(serverBuffer);
        pmm_free(clientBuffer);
        free(conn);
        return NULL;
    }
    wait_queue_init(&conn->waitQueue);
    lock_init(&conn->lock);
    atomic_init(&conn->ref, 1);

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
        pmm_free(conn->serverRing.buffer);
        pmm_free(conn->clientRing.buffer);
        file_deref(conn->listener);
        wait_queue_deinit(&conn->waitQueue);
        free(conn);
    }
}

// Note: The listener files are only used within the kernel, thus it should not have any operations defined.
SYSFS_STANDARD_SYSOBJ_OPEN_DEFINE(local_listen_open, (file_ops_t){0});

static void local_listen_on_free(sysobj_t* obj)
{
    local_socket_t* local = obj->private;
    for (uint64_t i = local->listen.readIndex; i != local->listen.writeIndex; i = (i + 1) % LOCAL_BACKLOG_MAX)
    {
        local_connection_deref(local->listen.backlog[i]);
    }
    wait_queue_deinit(&local->listen.waitQueue);
    free(local);
}

static sysobj_ops_t localListenOps = {
    .open = local_listen_open,
    .onFree = local_listen_on_free,
};

static uint64_t local_socket_init(socket_t* socket)
{
    local_socket_t* local = malloc(sizeof(local_socket_t));
    if (local == NULL)
    {
        return ERR;
    }
    local->state = LOCAL_SOCKET_BLANK;
    local->address[0] = '\0';
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
    lock_release(&local->lock);

    if (WAITSYS_BLOCK_LOCK(&local->listen.waitQueue, &local->lock, local->listen.length != 0) != BLOCK_NORM)
    {
        lock_release(&local->lock);
        return 0;
    }

    local_socket_t* newLocal = malloc(sizeof(local_socket_t));
    if (newLocal == NULL)
    {
        lock_release(&local->lock);
        return ERR;
    }
    newLocal->state = LOCAL_SOCKET_ACCEPT;
    newLocal->address[0] = '\0';
    newLocal->accept.conn = local_connection_ref(local->listen.backlog[local->listen.readIndex]);
    local->listen.readIndex = (local->listen.readIndex + 1) % LOCAL_BACKLOG_MAX;
    local->listen.length--;
    lock_init(&newLocal->lock);
    newSocket->private = newLocal;

    lock_release(&local->lock);
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
        sysobj_free(local->listen.obj);
    }
    break;
    case LOCAL_SOCKET_CONNECT:
    {
        local_connection_deref(local->connect.conn);
        free(local);
    }
    break;
    case LOCAL_SOCKET_ACCEPT:
    {
        local_connection_deref(local->accept.conn);
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

    strcpy(local->address, address);
    local->state = LOCAL_SOCKET_BOUND;
    return 0;
}

// TODO: Implement backlog length
static uint64_t local_socket_listen(socket_t* socket)
{
    local_socket_t* local = socket->private;
    LOCK_DEFER(&local->lock);

    if (local->state != LOCAL_SOCKET_BOUND)
    {
        return ERROR(ENOOP);
    }

    local->listen.obj = sysobj_new("/net/local/listen", local->address, &localListenOps, local);
    if (local->listen.obj == NULL)
    {
        return ERR;
    }
    local->listen.readIndex = 0;
    local->listen.writeIndex = 0;
    local->listen.length = 0;
    wait_queue_init(&local->listen.waitQueue);
    local->state = LOCAL_SOCKET_LISTEN;
    return 0;
}

static uint64_t local_socket_connect(socket_t* socket, const char* address)
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

    local_connection_t* conn = local_connection_new(address); // First ref
    if (conn == NULL)
    {
        return ERR;
    }

    local_socket_t* listener = conn->listener->private;
    LOCK_DEFER(&listener->lock);

    if (listener->listen.length == LOCAL_BACKLOG_MAX)
    {
        local_connection_deref(conn);
        return ERROR(EBUSY);
    }

    listener->listen.backlog[listener->listen.writeIndex] = conn; // First ref
    listener->listen.writeIndex = (listener->listen.writeIndex + 1) % LOCAL_BACKLOG_MAX;
    listener->listen.length++;
    waitsys_unblock(&listener->listen.waitQueue, UINT64_MAX);

    local->connect.conn = local_connection_ref(conn);
    local->state = LOCAL_SOCKET_CONNECT;
    return 0;
}

static uint64_t local_socket_send(socket_t* socket, const void* buffer, uint64_t count)
{
    if (count == 0 || count >= PAGE_SIZE / 2)
    {
        return ERROR(EINVAL);
    }

    local_socket_t* local = socket->private;
    LOCK_DEFER(&local->lock);

    ring_t* ring;
    local_connection_t* conn;
    switch (local->state)
    {
    case LOCAL_SOCKET_CONNECT:
    {
        ring = &local->connect.conn->serverRing;
        conn = local->connect.conn;
    }
    break;
    case LOCAL_SOCKET_ACCEPT:
    {
        ring = &local->accept.conn->clientRing;
        conn = local->accept.conn;
    }
    break;
    default:
    {
        return ERROR(ENOOP);
    }
    }

    if (count >= PAGE_SIZE / 2)
    {
        return ERROR(EINVAL);
    }

    if (WAITSYS_BLOCK_LOCK(&conn->waitQueue, &conn->lock, ring_free_length(ring) >= count + sizeof(uint64_t) || atomic_load(&conn->ref) == 1) != BLOCK_NORM)
    {
        lock_release(&conn->lock);
        return 0;
    }

    if (atomic_load(&conn->ref) == 1)
    {
        lock_release(&conn->lock);
        return 0;
    }

    ring_write(ring, &count, sizeof(uint64_t));
    ring_write(ring, buffer, count);

    waitsys_unblock(&conn->waitQueue, UINT64_MAX);
    lock_release(&conn->lock);
    return count;
}

static uint64_t local_socket_receive(socket_t* socket, void* buffer, uint64_t count, uint64_t offset, bool* endOfSegment)
{
    if (count == 0 || count >= PAGE_SIZE / 2)
    {
        return ERROR(EINVAL);
    }

    local_socket_t* local = socket->private;
    LOCK_DEFER(&local->lock);

    ring_t* ring;
    local_connection_t* conn;
    switch (local->state)
    {
    case LOCAL_SOCKET_CONNECT:
    {
        ring = &local->connect.conn->clientRing;
        conn = local->connect.conn;
    }
    break;
    case LOCAL_SOCKET_ACCEPT:
    {
        ring = &local->accept.conn->serverRing;
        conn = local->accept.conn;
    }
    break;
    default:
    {
        return ERROR(ENOOP);
    }
    }

    if (WAITSYS_BLOCK_LOCK(&conn->waitQueue, &conn->lock, ring_data_length(ring) != 0 || atomic_load(&conn->ref) == 1) != BLOCK_NORM)
    {
        lock_release(&conn->lock);
        return 0;
    }

    if (atomic_load(&conn->ref) == 1)
    {
        lock_release(&conn->lock);
        return 0;
    }

    uint64_t availCount;
    ring_read_at(ring, 0, &availCount, sizeof(uint64_t));
    uint64_t readCount = (offset <= availCount) ? MIN(count, availCount - offset) : 0;
    ring_read_at(ring, sizeof(uint64_t) + offset, buffer, readCount);

    if (sizeof(uint64_t) + offset + count >= availCount)
    {
        ring_move_read_forward(ring, sizeof(uint64_t) + availCount);
        *endOfSegment = true;
    }

    waitsys_unblock(&conn->waitQueue, UINT64_MAX);
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
        poll->occurred = POLL_READ & (local->listen.length != 0);
        return &local->listen.waitQueue;
    }
    break;
    case LOCAL_SOCKET_CONNECT:
    {
        LOCK_DEFER(&local->connect.conn->lock);
        poll->occurred = POLL_READ & (ring_data_length(&local->connect.conn->clientRing) != 0 || atomic_load(&local->connect.conn->ref) == 1);
        return &local->connect.conn->waitQueue;
    }
    break;
    case LOCAL_SOCKET_ACCEPT:
    {
        LOCK_DEFER(&local->accept.conn->lock);
        poll->occurred = POLL_READ & (ring_data_length(&local->accept.conn->serverRing) != 0 || atomic_load(&local->accept.conn->ref) == 1);
        return &local->accept.conn->waitQueue;
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
    ASSERT_PANIC(socket_family_expose(&family) != NULL);
}
