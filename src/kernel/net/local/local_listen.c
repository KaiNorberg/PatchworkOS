#include <kernel/net/local/local_listen.h>

#include <kernel/fs/sysfs.h>
#include <kernel/log/panic.h>
#include <kernel/net/local/local.h>
#include <kernel/net/local/local_conn.h>
#include <kernel/net/socket_family.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>

#include <stdlib.h>
#include <sys/list.h>

static dentry_t* listenDir = NULL;

static map_t listeners;
static rwlock_t listenersLock;

void local_listen_dir_init(void)
{
    socket_family_t* family = socket_family_get("local");
    if (family == NULL)
    {
        panic(NULL, "Failed to get local socket family");
    }

    listenDir = sysfs_dir_new(family->dir, "listen", NULL, NULL);
    if (listenDir == NULL)
    {
        panic(NULL, "Failed to create local listen dir");
    }

    map_init(&listeners);
    rwlock_init(&listenersLock);
}

local_listen_t* local_listen_new(const char* address)
{
    if (address == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    local_listen_t* listen = malloc(sizeof(local_listen_t));
    if (listen == NULL)
    {
        return NULL;
    }

    ref_init(&listen->ref, local_listen_free);
    map_entry_init(&listen->entry);
    strncpy(listen->address, address, MAX_NAME - 1);
    listen->address[MAX_NAME - 1] = '\0';
    list_init(&listen->backlog);
    listen->pendingAmount = 0;
    listen->maxBacklog = LOCAL_MAX_BACKLOG;
    listen->isClosed = false;
    lock_init(&listen->lock);
    wait_queue_init(&listen->waitQueue);
    listen->file = sysfs_file_new(listenDir, listen->address, NULL, NULL, listen);
    if (listen->file == NULL)
    {
        wait_queue_deinit(&listen->waitQueue);
        free(listen);
        return NULL;
    }

    RWLOCK_WRITE_SCOPE(&listenersLock);

    map_key_t key = map_key_string(listen->address);
    if (map_get(&listeners, &key) != NULL)
    {
        DEREF(listen->file);
        wait_queue_deinit(&listen->waitQueue);
        free(listen);

        errno = EADDRINUSE;
        return NULL;
    }

    if (map_insert(&listeners, &key, &listen->entry) == ERR)
    {
        DEREF(listen->file);
        wait_queue_deinit(&listen->waitQueue);
        free(listen);
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

    DEREF(listen->file);

    rwlock_write_acquire(&listenersLock);
    map_remove(&listeners, &listen->entry);
    rwlock_write_release(&listenersLock);

    local_conn_t* temp;
    local_conn_t* conn;
    LIST_FOR_EACH_SAFE(conn, temp, &listen->backlog, entry)
    {
        lock_acquire(&conn->lock);
        conn->isClosed = true;
        wait_unblock(&conn->waitQueue, WAIT_ALL, EOK);
        lock_release(&conn->lock);
    }

    wait_queue_deinit(&listen->waitQueue);
    free(listen);
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
    return REF(listen);
}
