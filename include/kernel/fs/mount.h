#pragma once

#include <kernel/fs/path.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>
#include <stdint.h>
#include <sys/list.h>
#include <sys/map.h>

typedef struct mount mount_t;
typedef struct volume volume_t;
typedef struct dentry dentry_t;
typedef struct path path_t;

/**
 * @brief Mount point.
 * @defgroup kernel_fs_mount Mount
 * @ingroup kernel_fs
 *
 * A mount represents a link between two locations within the VFS hierarchy.
 *
 * Typically, this might be a link from an arbitrary directory to the root of a volume. However, it can also be a
 * link from any arbitrary location to any other arbitrary location, in which case it is referred to as a bind mount.
 *
 * @{
 */

/**
 * @brief Check if a mount is a root mount within its namespace.
 *
 * @param mount The mount to check.
 * @return `true` if the mount is a root mount, `false` otherwise.
 */
#define MOUNT_IS_ROOT(mount) ((mount)->parent == NULL)

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
    dentry_t* source; ///< The dentry to appear at the target, usually the root of the mounted filesystem.
    dentry_t* target; ///< The dentry which the source is mounted to, can be `NULL` for the root filesystem.
    volume_t* volume; ///< The volume of the mounted filesystem.
    mount_t* parent;  ///< The parent mount, can be `NULL` for the root filesystem.
    mode_t mode;      ///< Specifies the maximum permissions for this mount and if it is a directory or a file.
    rcu_entry_t rcu;  ///< RCU entry for deferred cleanup.
} mount_t;

/**
 * @brief Create a new mount.
 *
 * This does not add the mount to the mount cache, that must be done separately with `vfs_add_mount()`.
 *
 * There is no `mount_free()` instead use `UNREF()`.
 *
 * @param volume The volume of the mounted filesystem.
 * @param source The dentry to appear at target once mounted, usually the root dentry of the mounted filesystem.
 * @param target The dentry which the source is mounted to, can be `NULL` for the root filesystem.
 * @param parent The parent mount, can be `NULL` for the root filesystem.
 * @param mode Specifies the maximum permissions for this mount and if it is a directory or a file.
 * @return On success, the new mount. On failure, returns `NULL`.
 */
mount_t* mount_new(volume_t* volume, dentry_t* source, dentry_t* target, mount_t* parent, mode_t mode);

/** @} */
