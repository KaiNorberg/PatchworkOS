#pragma once

#include "fs/sysfs.h"

/**
 * @brief Networking and Sockets.
 * @defgroup kernel_net Networking
 * @ingroup kernel
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
 * @return Pointer to the networking sysfs directory (`/net`)
 */
dentry_t* net_get_dir(void);

/** @} */
