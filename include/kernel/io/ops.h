#pragma once

#include <kernel/config.h>
#include <kernel/io/irp.h>

/**
 * @brief I/O Operations.
 * @defgroup kernel_io_ops I/O Operations
 * @ingroup kernel_io
 *
 * @{
 */

/**
 * @brief Dispatch an I/O request created by the I/O Ring system to the appropriate handler.
 *
 * @param irp The I/O request packet to dispatch.
 */
void io_op_dispatch(irp_t* irp);

/** @} */