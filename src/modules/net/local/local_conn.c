#include "local_conn.h"
#include "local.h"
#include "local_listen.h"

#include <kernel/fs/devfs.h>
#include <kernel/io/irp.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <stdlib.h>
#include <sys/list.h>

local_conn_t* local_conn_new(local_listen_t* listen)
{
    if (listen == NULL)
    {
        return NULL;
    }

    local_conn_t* conn = malloc(sizeof(local_conn_t));
    if (conn == NULL)
    {
        return NULL;
    }

    ref_init(&conn->ref, local_conn_free);
    list_entry_init(&conn->entry);
    FIFO_DEFINE_INIT(conn->s2cFifo);
    FIFO_DEFINE_INIT(conn->c2sFifo);
    conn->listen = REF(listen);
    conn->isClosed = false;
    lock_init(&conn->lock);
    list_init(&conn->c2sReaders);
    list_init(&conn->c2sWriters);
    list_init(&conn->s2cReaders);
    list_init(&conn->s2cWriters);
    list_init(&conn->polls);
    return conn;
}

void local_conn_free(local_conn_t* conn)
{
    if (conn == NULL)
    {
        return;
    }

    if (conn->listen != NULL)
    {
        UNREF(conn->listen);
    }

    local_conn_close(conn);

    free(conn);
}

void local_conn_close(local_conn_t* conn)
{
    if (conn == NULL)
    {
        return;
    }

    lock_acquire(&conn->lock);
    if (conn->isClosed)
    {
        lock_release(&conn->lock);
        return;
    }

    conn->isClosed = true;

    list_t pending = LIST_CREATE(pending);
    list_splice(&pending, &conn->c2sReaders);
    list_splice(&pending, &conn->c2sWriters);
    list_splice(&pending, &conn->s2cReaders);
    list_splice(&pending, &conn->s2cWriters);
    list_splice(&pending, &conn->polls);
    lock_release(&conn->lock);

    while (!list_is_empty(&pending))
    {
        irp_t* irp = CONTAINER_OF(list_pop_front(&pending), irp_t, entry);
        irp_cancel(irp);
    }
}