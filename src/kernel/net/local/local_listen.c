#include "local_listen.h"

#include "local_conn.h"

#include "fs/sysfs.h"
#include "mem/heap.h"
#include "sched/wait.h"
#include "log/panic.h"
#include "sync/lock.h"

#include <_internal/MAX_NAME.h>
#include <sys/list.h>

static sysfs_dir_t listenDir;

void local_listen_dir_init(void)
{
    if (sysfs_dir_init(&listenDir, &listenDir, "listen", NULL, NULL) == ERR)
    {
        panic(NULL, "Failed to create local listen dir");
    }
}

local_listen_t* local_listen_new(const char* address, uint32_t maxBacklog)
{
    if (address == NULL || maxBacklog == 0)
    {
        errno = EINVAL;
        return NULL;
    }

    local_listen_t* listen = heap_alloc(sizeof(local_listen_t), HEAP_NONE);
    if (listen == NULL)
    {
        return NULL;
    }

    list_entry_init(&listen->entry);
    strncpy(listen->address, address, MAX_NAME);
    listen->address[MAX_NAME - 1] = '\0';
    list_init(&listen->backlog);
    listen->maxBacklog = maxBacklog;
    atomic_init(&listen->ref, 1);
    atomic_init(&listen->isClosed, false);
    lock_init(&listen->lock);
    wait_queue_init(&listen->waitQueue);

    if (sysfs_file_init(&listen->file, &listenDir, listen->address, NULL, NULL, listen) == ERR)
    {
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

    sysfs_file_deinit(&listen->file);

    local_conn_t* temp;
    local_conn_t* conn;
    LIST_FOR_EACH_SAFE(conn, temp, &listen->backlog, entry)
    {
        atomic_store(&conn->isClosed, true);
        wait_unblock(&conn->waitQueue, WAIT_ALL);
    }

    wait_queue_deinit(&listen->waitQueue);

    lock_release(&listen->lock);
    heap_free(listen);
}

local_listen_t* local_listen_ref(local_listen_t* listen)
{
    if (listen != NULL)
    {
        atomic_fetch_add_explicit(&listen->ref, 1, memory_order_relaxed);
    }
    return listen;
}

void local_listen_deref(local_listen_t* listen)
{
    if (listen == NULL)
    {
        return;
    }

    uint64_t ref = atomic_fetch_sub_explicit(&listen->ref, 1, memory_order_relaxed);
    if (ref <= 1)
    {
        atomic_thread_fence(memory_order_acquire);
        assert(ref == 1); // Check for double free.
        local_listen_free(listen);
    }
}
