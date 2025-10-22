#pragma once

#include "fs/sysfs.h"

/**
 * @brief Networking and Sockets.
 * @defgroup kernel_net Networking
 * @ingroup kernel
 *
 * The networking subsystem is exposed as `/net`.
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
