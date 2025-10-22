#pragma once

/**
 * @brief Socket types.
 * @defgroup kernel_net_socket_type Socket Types
 * @ingroup kernel_net_socket
 *
 * All socket types should follow POSIX expectations.
 *
 * @{
 */

/**
 * @brief Socket type enumeration.
 * @enum socket_type_t
 */
typedef enum
{
    SOCKET_SEQPACKET = 1 << 0,
    SOCKET_TYPE_AMOUNT = 1,
} socket_type_t;

/**
 * @brief Convert a socket type to a string.
 *
 * @param type Socket type.
 * @return String representation of the socket type.
 */
const char* socket_type_to_string(socket_type_t type);

/** @} */
