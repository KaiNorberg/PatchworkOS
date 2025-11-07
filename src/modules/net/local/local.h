#pragma once

#include <kernel/sync/lock.h>

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct local_listen local_listen_t;
typedef struct local_conn local_conn_t;

/**
 * @brief Local Sockets.
 * @defgroup module_net_local Local Sockets
 * @ingroup module_net
 *
 * Local Sockets are similar to UNIX domain sockets, they allow local communication on the host in a server-client
 * manner.
 *
 * @{
 */

/**
 * @brief The size of local sockets buffer.
 */
#define LOCAL_BUFFER_SIZE (4 * PAGE_SIZE)

/**
 * @brief The maximum size of a packet allowed to be sent/received via local sockets.
 */
#define LOCAL_MAX_PACKET_SIZE (LOCAL_BUFFER_SIZE - sizeof(local_packet_header_t))

/**
 * @brief The maximum backlog of connections for a local listener.
 */
#define LOCAL_MAX_BACKLOG 128

/**
 * @brief Magic number for local socket packets, used for validation.
 */
#define LOCAL_PACKET_MAGIC 0xC0D74B56

/**
 * @brief Local packet header structure.
 * struct local_packet_header_t
 */
typedef struct
{
    uint32_t magic;
    uint32_t size;
} local_packet_header_t;

/**
 * @brief Local socket data structure.
 * struct local_socket_data_t
 *
 * Stored in the `private` field of a `socket_t` for local sockets.
 */
typedef struct
{
    union {
        struct
        {
            local_listen_t* listen;
        } listen;
        struct
        {
            local_conn_t* conn;
            bool isServer;
        } conn;
    };
    lock_t lock;
} local_socket_data_t;

/**
 * @brief Initialize the local networking subsystem.
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t net_local_init(void);

/**
 * @brief Deinitialize the local networking subsystem.
 */
void net_local_deinit(void);

/** @} */
