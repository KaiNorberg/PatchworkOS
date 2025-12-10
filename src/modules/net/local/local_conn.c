#include "local/local_conn.h"

#include "local/local.h"
#include "local/local_listen.h"
#include "socket.h"
#include "socket_family.h"
#include <kernel/fs/sysfs.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <stdlib.h>
#include <sys/list.h>

local_conn_t* local_conn_new(local_listen_t* listen)
{
    if (listen == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    local_conn_t* conn = malloc(sizeof(local_conn_t));
    if (conn == NULL)
    {
        return NULL;
    }

    conn->clientToServerBuffer = malloc(LOCAL_BUFFER_SIZE);
    if (conn->clientToServerBuffer == NULL)
    {
        free(conn);
        return NULL;
    }

    conn->serverToClientBuffer = malloc(LOCAL_BUFFER_SIZE);
    if (conn->serverToClientBuffer == NULL)
    {
        free(conn->clientToServerBuffer);
        free(conn);
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
        UNREF(conn->listen);
    }

    free(conn->clientToServerBuffer);
    free(conn->serverToClientBuffer);
    free(conn);
}
