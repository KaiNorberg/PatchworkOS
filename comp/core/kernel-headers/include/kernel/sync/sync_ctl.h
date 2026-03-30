#pragma once

#include <libc/sync.h>

/**
 * @brief Kernel-backed user-space synchronization control.
 * @defgroup kernel_sync_sync_ctl User-space Sync Control
 * @ingroup kernel_sync
 *
 * The user-space syncronization control system is inspired by Linux's `futex()` system, allowing user-space programs to
 * perform efficient synchronization by only involving the kernel when absolutely necessary.
 *
 * Each syncronization control object is specified via an address within the `SYS_SYNC_CTL` system call, this address
 * will be translated into its physical address to actually access the object. This means that if the specified address
 * is within shared memory both processes will be able to synchronize on the same object.
 *
 * @see [futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html)
 *
 * @{
 */

/** @} */
