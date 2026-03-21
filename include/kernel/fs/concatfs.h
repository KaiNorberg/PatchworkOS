#pragma once

#include <kernel/fs/vnode.h>
#include <stdlib.h>

/**
 * @brief Concatenated File System (concatfs)
 * @defgroup kernel_fs_concatfs concatfs
 * @ingroup kernel_fs
 *
 * The Concatenated File System (concatfs) allows multiple locations within the filesystem hierarchy
 * to appear as a single, unified directory by concatenating their contents without recursion.
 *
 * @note This is distinct from typical "UnionFS" or "OverlayFS" implementations in that it does apply to subdirectories
 * and that since it is blindly concatenating contents multiple files of the same name can appear in the same directory.
 * The gain is that it provides far more predictable behavior than traditional union file systems.
 *
 * ## Payload
 *
 * To create an instance of concatfs we utilize its clone file as all other filesystems with an additional payload
 * specifying a list of target file descriptors.
 *
 * Included below is an example of the path required for creating a concatfs instance:
 *
 * ```
 * /sys/fs/concatfs/clone?targets=1,2,3,4
 * ```
 *
 * @{
 */

/**
 * @brief Initialize the Concatenated File System (concatfs).
 */
void concatfs_init(void);

/** @} */
