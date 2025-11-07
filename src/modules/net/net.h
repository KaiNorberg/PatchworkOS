#pragma once

#include <kernel/fs/sysfs.h>

/**
 * @brief Networking and Sockets.
 * @defgroup module_net Networking
 * @ingroup modules
 *
 * The networking subsystem is exposed as `/net` and is responsible for providing networking and IPC through sockets.
 *
 * @see module_net_socket for information about sockets.
 *
 * @{
 */

/**
 * @brief Retrieve the mount for the networking subsystem.
 *
 * @return The mount for the networking subsystem (`/net`).
 */
mount_t* net_get_mount(void);

/** @} */
