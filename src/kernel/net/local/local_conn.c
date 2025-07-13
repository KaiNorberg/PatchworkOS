#include "local_conn.h"

#include "fs/sysfs.h"
#include "mem/heap.h"
#include "net/local/local.h"
#include "net/local/local_listen.h"
#include "net/socket.h"
#include "net/socket_family.h"
#include "sched/wait.h"
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
    if (conn == NULL)
    {
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

    ref_init(&conn->ref, local_conn_free);
    list_entry_init(&conn->entry);
    ring_init(&conn->serverToClient, conn->serverToClientBuffer, LOCAL_BUFFER_SIZE);
    ring_init(&conn->clientToServer, conn->clientToServerBuffer, LOCAL_BUFFER_SIZE);
    conn->listen = REF(listen);
    conn->isClosed = false;
    lock_init(&conn->lock);
    wait_queue_init(&conn->waitQueue);
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
        DEREF(conn->listen);
    }

    heap_free(conn->clientToServerBuffer);
    heap_free(conn->serverToClientBuffer);
    heap_free(conn);
}
