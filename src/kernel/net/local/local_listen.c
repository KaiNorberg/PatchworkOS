#include "local_listen.h"

#include "local_conn.h"

#include "fs/sysfs.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "net/local/local.h"
#include "net/socket_family.h"
#include "sched/wait.h"
#include "sync/lock.h"
#include "sync/rwlock.h"
#include "utils/map.h"

#include <_internal/MAX_NAME.h>
#include <sys/list.h>

static sysfs_dir_t listenDir;

static map_t listeners;
static rwlock_t listenersLock;

void local_listen_dir_init(socket_family_t* family)
{
    if (sysfs_dir_init(&listenDir, &family->dir, "listen", NULL, NULL) == ERR)
    {
        panic(NULL, "Failed to create local listen dir");
    }

    if (map_init(&listeners) == ERR)
    {
        panic(NULL, "Failed to initialize listeners map");
    }

    rwlock_init(&listenersLock);
}

local_listen_t* local_listen_new(const char* address)
{
    if (address == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    local_listen_t* listen = heap_alloc(sizeof(local_listen_t), HEAP_NONE);
    if (listen == NULL)
    {
        return NULL;
    }

    ref_init(&listen->ref, local_listen_free);
    map_entry_init(&listen->entry);
    strncpy(listen->address, address, MAX_NAME);
    listen->address[MAX_NAME - 1] = '\0';
    list_init(&listen->backlog);
    listen->maxBacklog = LOCAL_MAX_BACKLOG;
    listen->pendingAmount = 0;
    atomic_init(&listen->isClosed, true);
    lock_init(&listen->lock);
    wait_queue_init(&listen->waitQueue);

    if (sysfs_file_init(&listen->file, &listenDir, listen->address, NULL, NULL, listen) == ERR)
    {
        wait_queue_deinit(&listen->waitQueue);
        heap_free(listen);
        return NULL;
    }

    RWLOCK_WRITE_SCOPE(&listenersLock);

    map_key_t key = map_key_string(listen->address);

    if (map_get(&listeners, &key) != NULL)
    {
        sysfs_file_deinit(&listen->file);
        wait_queue_deinit(&listen->waitQueue);
        heap_free(listen);

        errno = EADDRINUSE;
        return NULL;
    }

    if (map_insert(&listeners, &key, &listen->entry) == ERR)
    {
        sysfs_file_deinit(&listen->file);
        wait_queue_deinit(&listen->waitQueue);
        heap_free(listen);
        return NULL;
    }

    return listen;
}

void local_listen_free(local_listen_t* listen)
{
    if (listen == NULL)
    {
        return;
    }

    lock_acquire(&listen->lock);

    rwlock_write_acquire(&listenersLock);
    map_key_t key = map_key_string(listen->address);
    map_remove(&listeners, &key);
    rwlock_write_release(&listenersLock);

    sysfs_file_deinit(&listen->file);

    local_conn_t* temp;
    local_conn_t* conn;
    LIST_FOR_EACH_SAFE(conn, temp, &listen->backlog, entry)
    {
        atomic_store(&conn->isClosed, true);
        wait_unblock(&conn->waitQueue, WAIT_ALL);
        ref_dec(conn);
    }
    list_init(&listen->backlog); // Reset list.

    wait_queue_deinit(&listen->waitQueue);

    lock_release(&listen->lock);
    heap_free(listen);
}

local_listen_t* local_listen_find(const char* address)
{
    RWLOCK_READ_SCOPE(&listenersLock);

    map_key_t key = map_key_string(address);
    map_entry_t* entry = map_get(&listeners, &key);
    if (entry == NULL)
    {
        return NULL;
    }

    local_listen_t* listen = CONTAINER_OF(entry, local_listen_t, entry);
    return ref_inc(listen);
}
