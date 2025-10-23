#pragma once

#include "fs/sysfs.h"

/**
 * @brief Networking and Sockets.
 * @defgroup kernel_net Networking
 * @ingroup kernel
 *
 * The networking subsystem is exposed as `/net` and is responsible for providing networking and IPC through sockets.
 *
 * @see kernel_net_socket for information about sockets.
 *
 * @{
 */

/**
 * @brief Initialize the networking subsystem.
 */
void net_init(void);

/**
 * @brief Retrieve the mount for the networking subsystem.
 *
 * @return The mount for the networking subsystem (`/net`).
 */
mount_t* net_get_mount(void);

/** @} */
