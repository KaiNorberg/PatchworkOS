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
    superblock_t* superblock; ///< The superblock of the mounted filesystem.
    dentry_t* mountpoint;     ///< The dentry that this filesystem is mounted on, can be `NULL` for the root filesystem.
    dentry_t* root;           ///< The root dentry of the mounted filesystem.
    mount_t* parent;          ///< The parent mount, can be `NULL` for the root filesystem.
    mode_t mode;              ///< The maximum permissions for this mount.
} mount_t;

/**
 * @brief Create a new mount.
 *
 * This does not add the mount to the mount cache, that must be done separately with `vfs_add_mount()`.
 *
 * There is no `mount_free()` instead use `UNREF()`.
 *
 * Note that the `root` dentry is not necessarily the same as `superblock->root`, instead its the directory that will
 * "appear" to be the root of the newly mounted filesystem, the dentry that gets jumped to during the lookup. This is
 * important for implementing bind mounts.
 *
 * @param superblock The superblock of the mounted filesystem.
 * @param root The root dentry of the mounted filesystem.
 * @param mountpoint The dentry that this filesystem will be mounted on, can be `NULL` for the root filesystem.
 * @param parent The parent mount, can be `NULL` for the root filesystem.
 * @param mode The maximum allowed permissions for files/directories opened under this mount.
 * @return On success, the new mount. On failure, returns `NULL`.
 */
mount_t* mount_new(superblock_t* superblock, dentry_t* root, dentry_t* mountpoint, mount_t* parent, mode_t mode);

/** @} */
