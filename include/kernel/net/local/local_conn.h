#pragma once

#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ref.h>
#include <kernel/utils/ring.h>

#include <sys/io.h>
#include <sys/list.h>

typedef struct local_listen local_listen_t;

/**
 * @brief Local Connections.
 * @defgroup kernel_net_local_conn Local Connections
 * @ingroup kernel_net_local
 *
 * Local connections represents a "link" between a listener and a client socket. They provide two-way communication
 * channels using ring buffers.
 *
 * @{
 */

/**
 * @brief Local Connection structure.
 * @struct local_conn_t
 */
typedef struct local_conn
{
    ref_t ref;
    list_entry_t entry;
    ring_t clientToServer;
    void* clientToServerBuffer;
    ring_t serverToClient;
    void* serverToClientBuffer;
    local_listen_t* listen;
    bool isClosed;
    lock_t lock;
    wait_queue_t waitQueue;
} local_conn_t;

/**
 * @brief Allocate and initialize a new local connection.
 *
 * @param listen Pointer to the local listener this connection is associated with.
 * @return On success, a pointer to the new local connection. On failure, `NULL` and `errno` is set.
 */
local_conn_t* local_conn_new(local_listen_t* listen);

/**
 * @brief Free and deinitialize a local connection.
 *
 * @param conn Pointer to the local connection to free.
 */
void local_conn_free(local_conn_t* conn);

/** @} */
