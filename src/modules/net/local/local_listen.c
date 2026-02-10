#include "local_listen.h"
#include "local.h"
#include "local_conn.h"

#include <kernel/fs/devfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>
#include <sys/status.h>

#include <stdlib.h>
#include <sys/list.h>

static bool local_listen_cmp(map_entry_t* entry, const void* key)
{
    local_listen_t* listen = CONTAINER_OF(entry, local_listen_t, entry);
    return strcmp(listen->address, (const char*)key) == 0;
}

static MAP_CREATE(listeners, 64, local_listen_cmp);
static rwlock_t listenersLock = RWLOCK_CREATE();

status_t local_listen_new(const char* address, local_listen_t** out)
{
    if (address == NULL || *address == '\0' || out == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    local_listen_t* listen = malloc(sizeof(local_listen_t));
    if (listen == NULL)
    {
        return ERR(PROTO, NOMEM);
    }

    ref_init(&listen->ref, local_listen_free);
    map_entry_init(&listen->entry);
    strncpy(listen->address, address, sizeof(listen->address));
    listen->address[sizeof(listen->address) - 1] = '\0';
    list_init(&listen->backlog);
    listen->pendingAmount = 0;
    listen->maxBacklog = LOCAL_MAX_BACKLOG;
    listen->isClosed = false;
    lock_init(&listen->lock);
    wait_queue_init(&listen->waitQueue);

    RWLOCK_WRITE_SCOPE(&listenersLock);

    uint64_t hash = hash_string(listen->address);
    if (map_find(&listeners, listen->address, hash) != NULL)
    {
        wait_queue_deinit(&listen->waitQueue);
        free(listen);

        return ERR(PROTO, ADDRINUSE);
    }

    map_insert(&listeners, &listen->entry, hash);

    *out = listen;
    return OK;
}

void local_listen_free(local_listen_t* listen)
{
    if (listen == NULL)
    {
        return;
    }

    rwlock_write_acquire(&listenersLock);
    uint64_t hash = hash_string(listen->address);
    map_remove(&listeners, &listen->entry, hash);
    rwlock_write_release(&listenersLock);

    local_conn_t* temp;
    local_conn_t* conn;
    LIST_FOR_EACH_SAFE(conn, temp, &listen->backlog, entry)
    {
        list_remove(&conn->entry);
        lock_acquire(&conn->lock);
        conn->isClosed = true;
        wait_unblock(&conn->waitQueue, WAIT_ALL, OK);
        lock_release(&conn->lock);
        UNREF(conn);
    }

    wait_queue_deinit(&listen->waitQueue);
    free(listen);
}

status_t local_listen_find(const char* address, local_listen_t** out)
{
    if (address == NULL || *address == '\0' || out == NULL)
    {
        return ERR(PROTO, INVAL);
    }

    RWLOCK_READ_SCOPE(&listenersLock);

    uint64_t hash = hash_string(address);
    map_entry_t* entry = map_find(&listeners, address, hash);
    if (entry == NULL)
    {
        return ERR(PROTO, NOENT);
    }

    local_listen_t* listen = CONTAINER_OF(entry, local_listen_t, entry);
    *out = REF(listen);
    return OK;
}
