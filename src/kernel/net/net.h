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
 * @brief Retrieve the sysfs directory for networking.
 *
 * @param out Output pointer to store the networking directory path (`/net`).
 */
void net_get_dir(path_t* out);

/** @} */
