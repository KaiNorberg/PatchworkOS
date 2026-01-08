#pragma once

#include <kernel/sync/lock.h>

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct local_listen local_listen_t;
typedef struct local_conn local_conn_t;

/**
 * @brief Local Protocol.
 * @defgroup module_net_local Local Protocol
 * @ingroup module_net
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
 * struct local_socket_t
 *
 * Stored in the `private` field of a `socket_t` for local sockets.
 */
typedef struct
{
    local_listen_t* listen;
    local_conn_t* conn;
    bool isServer;
} local_socket_t;

/** @} */
