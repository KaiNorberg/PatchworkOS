#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/devfs.h>
#include <kernel/proc/process.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>

#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

/**
 * @brief Filesystem interface.
 * @defgroup kernel_fs_filesystem Filesystem
 * @ingroup kernel_fs
 *
 * @{
 */

/**
 * @brief Filesystem structure, represents a filesystem type, e.g. fat32, tmpfs, devfs, etc.
 *
 * @todo Add safety for if a module defining a filesystem is unloaded.
 */
typedef struct filesystem
{
    map_entry_t mapEntry; ///< Used internally.
    list_t superblocks;   ///< Used internally.
    rwlock_t lock;        ///< Used internally.
    const char* name;
    dentry_t* (*mount)(filesystem_t* fs, block_device_t* device, void* private);
} filesystem_t;

/**
 * @brief Registers a filesystem.
 *
 * @param fs The filesystem to register.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - Other values from `map_insert()`.
 */
uint64_t filesystem_register(filesystem_t* fs);

/**
 * @brief Unregisters a filesystem.
 *
 * @param fs The filesystem to unregister, or `NULL` for no-op.
 */
void filesystem_unregister(filesystem_t* fs);

/**
 * @brief Gets a filesystem by name.
 *
 * @param name The name of the filesystem.
 * @return On success, the filesystem. On failure, returns `NULL`.
 */
filesystem_t* filesystem_get(const char* name);

/** @} */