#pragma once

#include <kernel/fs/sysfs.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct socket_family socket_family_t;

/**
 * @brief Local Listeners.
 * @defgroup kernel_net_local_listen Local Listeners
 * @ingroup kernel_net_local
 *
 * Local listeners can be thought of as servers that wait for incoming connections on a specified named address. All
 * local listeners are listed in the `/net/local/listen/` directory, but they are not directly accessible via the
 * filesystem. Instead, they are managed through socket operations.
 *
 * @{
 */

/**
 * @brief Local Listener structure.
 * @struct local_listen_t
 */
typedef struct local_listen
{
    ref_t ref;
    map_entry_t entry;
    char address[MAX_NAME];
    list_t backlog;
    uint32_t pendingAmount;
    uint32_t maxBacklog;
    bool isClosed;
    lock_t lock;
    wait_queue_t waitQueue;
    dentry_t* file;
} local_listen_t;

/**
 * @brief Initialize the `/net/local/listen/` directory.
 */
void local_listen_dir_init(void);

/**
 * @brief Allocate and initialize a new local listener.
 *
 * @param address Address to listen on.
 * @return On success, a pointer to the new local listener. On failure, `NULL` and `errno` is set.
 */
local_listen_t* local_listen_new(const char* address);

/**
 * @brief Free and deinitialize a local listener.
 *
 * @param listen Pointer to the local listener to free.
 */
void local_listen_free(local_listen_t* listen);

/**
 * @brief Find a local listener by its address.
 *
 * @param address Address of the local listener to find.
 * @return On success, reference to the local listener. On failure, `NULL` and `errno` is set.
 */
local_listen_t* local_listen_find(const char* address);

/** @} */
