#include "local_conn.h"

#include "fs/sysfs.h"
#include "mem/heap.h"
#include "net/local/local.h"
#include "net/local/local_listen.h"
#include "sched/wait.h"
#include "net/socket_family.h"
#include "net/socket.h"
#include "sync/lock.h"

#include <_internal/MAX_NAME.h>
#include <sys/list.h>

local_conn_t* local_conn_new(local_listen_t* listen)
{
    if (listen == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    local_conn_t* conn = heap_alloc(sizeof(local_conn_t), HEAP_NONE);
    if (conn == NULL) {
        return NULL;
    }

    conn->clientToServerBuffer = heap_alloc(LOCAL_BUFFER_SIZE, HEAP_VMM);
    if (conn->clientToServerBuffer == NULL)
    {
        heap_free(conn);
        return NULL;
    }

    conn->serverToClientBuffer = heap_alloc(LOCAL_BUFFER_SIZE, HEAP_VMM);
    if (conn->serverToClientBuffer == NULL)
    {
        heap_free(conn->clientToServerBuffer);
        heap_free(conn);
        return NULL;
    }

    ring_init(&conn->serverToClient, conn->serverToClientBuffer, LOCAL_BUFFER_SIZE);
    ring_init(&conn->clientToServer, conn->clientToServerBuffer, LOCAL_BUFFER_SIZE);

    conn->listen = local_listen_ref(listen);
    atomic_init(&conn->ref, 1);
    atomic_init(&conn->isClosed, false);
    lock_init(&conn->lock);

    return conn;
}

void local_conn_free(local_conn_t* conn)
{
    if (conn == NULL)
    {
        errno = EINVAL;
        return;
    }

    if (conn->listen != NULL)
    {
        local_listen_deref(conn->listen);
    }

    heap_free(conn->clientToServerBuffer);
    heap_free(conn->serverToClientBuffer);
    heap_free(conn);
}

local_conn_t* local_conn_ref(local_conn_t* conn)
{
    if (conn != NULL)
    {
        atomic_fetch_add_explicit(&conn->ref, 1, memory_order_relaxed);
    }
    return conn;
}

void local_conn_deref(local_conn_t* conn)
{
    if (conn == NULL)
    {
        return;
    }

    uint64_t ref = atomic_fetch_sub_explicit(&conn->ref, 1, memory_order_relaxed);
    if (ref <= 1)
    {
        atomic_thread_fence(memory_order_acquire);
        assert(ref == 1); // Check for double free.
        local_conn_free(conn);
    }
}
