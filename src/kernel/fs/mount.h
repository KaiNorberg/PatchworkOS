#pragma once

#include "utils/map.h"
#include "utils/ref.h"

#include <stdatomic.h>
#include <stdint.h>

typedef struct mount mount_t;
typedef struct superblock superblock_t;
typedef struct dentry dentry_t;
typedef struct path path_t;

/**
 * @brief Mount point.
 * @defgroup kernel_fs_mount Mount
 * @ingroup kernel_fs
 *
 * A mount represents a location that a superblock is mounted to. It links a superblock (the mounted filesystem) to a
 * mountpoint (a dentry in another filesystem).
 *
 * @{
 */

/**
 * @brief Mount ID type.
 */
typedef uint64_t mount_id_t;
/**
 * @brief Mount structure.
 * @struct mount_t
 *
 * Mounts are owned by the VFS, not the filesystem.
 */
typedef struct mount
{
    ref_t ref;
    mount_id_t id;
    map_entry_t mapEntry;
    superblock_t* superblock; ///< The superblock of the mounted filesystem.
    dentry_t* dentry;         ///< The dentry that this filesystem is mounted on, can be `NULL` for the root filesystem.
    mount_t* parent;          ///< The parent mount, can be `NULL` for the root filesystem.
} mount_t;

/**
 * @brief Create a new mount.
 *
 * This does not add the mount to the mount cache, that must be done separately with `vfs_add_mount()`.
 *
 * There is no `mount_free()` instead use `DEREF()`.
 *
 * @param superblock The superblock of the mounted filesystem.
 * @param mountpoint The path to the mountpoint, can be NULL for the root filesystem.
 * @return On success, the new mount. On failure, returns `NULL` and `errno` is set.
 */
mount_t* mount_new(superblock_t* superblock, path_t* mountpoint);

/** @} */
