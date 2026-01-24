#pragma once

#include <kernel/io/irp.h>

/**
 * @brief I/O Request Packet cleanup.
 * @defgroup kernel_io_irp_cleanup I/O Request Packet cleanup.
 * @ingroup kernel_io
 *
 * @{
 */

/**
 * @brief Free the resources used by an IRP frames arguments.
 *
 * @param frame The frame to clean up.
 */
void irp_cleanup_args(irp_frame_t* frame);

/** @} */