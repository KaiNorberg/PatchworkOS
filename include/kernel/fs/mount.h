#pragma once

#include <kernel/fs/path.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>
#include <sys/list.h>

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
    dentry_t*
        source; ///< The dentry to appear at target once mounted, usually the root dentry of the mounted filesystem.
    dentry_t* target;         ///< The dentry which the source is mounted to, can be `NULL` for the root filesystem.
    superblock_t* superblock; ///< The superblock of the mounted filesystem.
    mount_t* parent;          ///< The parent mount, can be `NULL` for the root filesystem.
    mode_t mode;              ///< Specifies the maximum permissions for this mount and if it is a directory or a file.
} mount_t;

/**
 * @brief Create a new mount.
 *
 * This does not add the mount to the mount cache, that must be done separately with `vfs_add_mount()`.
 *
 * There is no `mount_free()` instead use `UNREF()`.
 *
 * @param superblock The superblock of the mounted filesystem.
 * @param source The dentry to appear at target once mounted, usually the root dentry of the mounted filesystem.
 * @param target The dentry which the source is mounted to, can be `NULL` for the root filesystem.
 * @param parent The parent mount, can be `NULL` for the root filesystem.
 * @param mode Specifies the maximum permissions for this mount and if it is a directory or a file.
 * @return On success, the new mount. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: Source or target dentry is negative.
 * - `ENOMEM`: Out of memory.
 */
mount_t* mount_new(superblock_t* superblock, dentry_t* source, dentry_t* target, mount_t* parent, mode_t mode);

/** @} */
