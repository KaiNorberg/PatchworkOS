#include <kernel/fs/devfs.h>
#include <kernel/fs/vfs.h>
#include <kernel/io/irp.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sched.h>

#include <stdint.h>
#include <string.h>
#include <sys/io.h>
#include <sys/status.h>

/**
 * @brief Constant devices
 * @defgroup kernel_drivers_const Constant Devices
 * @ingroup kernel_drivers
 *
 * This module provides the constant devices which provide user space with its primary means of allocating memory and
 * obtaining constant data.
 *
 * The constant devices are exposed under the `/dev/const/` directory:
 * - `/dev/const/one`: A readable and mappable file that returns bytes with all bits set to 1.
 * - `/dev/const/zero`: A readable and mappable file that returns bytes with all bits set to 0.
 * - `/dev/const/null`: A readable and writable file that discards all written data and returns EOF on read.
 *
 * @{
 */

/**
 * @brief Initialize the constant devices.
 */
void const_init(void);

/** @} */