#pragma once

#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct namespace namespace_t;
typedef struct mount mount_t;
typedef struct process process_t;

/**
 * @brief Per-process Namespaces.
 * @defgroup kernel_fs_namespace Namespaces
 * @ingroup kernel_fs
 *
 * The per-process namespace system allows each process to have its own view of the filesystem hierarchy.
 *
 * ## Propagation
 *
 * When a new mount or bind is created in a namespace or, it is only added to that specific namespace. Same concept
 * applies when unmounting.
 *
 * However, its possible to propagate mounts and unmounts to children and/or parent namespaces using mount flags
 * (`mount_flags_t`), this allows those namespaces to also see the new mount or bind or have the mount or bind removed.
 *
 * @{
 */

/**
 * @brief A mount in a namespace.
 * @struct namespace_mount
 *
 * Used to allow a single mount to exist in multiple namespaces.
 */
typedef struct namespace_mount
{
    list_entry_t entry;
    map_entry_t mapEntry;
    mount_t* mount;
    mount_flags_t flags;
} namespace_mount_t;

/**
 * @brief Namespace member flags.
 * @enum namespace_member_flags_t
 */
typedef enum
{
    NAMESPACE_MEMBER_SHARE = 0 << 0, ///< Share the same namespace as the source.
    NAMESPACE_MEMBER_COPY =
        1 << 1, ///< Copy the contents of the source into a new namespace with the source as the parent.
} namespace_member_flags_t;

/**
 * @brief Per-process namespace member.
 * @struct namespace_member_t
 *
 * Stored in each process, used to allow multiple processes to share a namespace.
 */
typedef struct namespace_member
{
    list_entry_t entry;
    namespace_t* ns;
    rwlock_t lock;
} namespace_member_t;

/**
 * @brief Namespace structure.
 * @struct namespace_t
 */
typedef struct namespace
{
    list_entry_t entry;  ///< The member for the parent's children list.
    list_t children;     ///< List of child namespaces.
    namespace_t* parent; ///< The parent namespace, can be `NULL`.
    list_t mounts;       ///< List of mounts in this namespace.
    map_t mountMap;      ///< Map used to go from source dentries to namespace mounts.
    mount_t* root;       ///< The root mount of the namespace.
    list_t members;      ///< List of `namespace_member_t`.
    rwlock_t lock;
    // clang-format off
} namespace_t;
// clang-format on

/**
 * @brief Initializes a namespace member.
 *
 * @param member The namespace member to initialize.
 * @param source The source namespace member to copy from or share a namespace with, or `NULL` to create a new empty
 * namespace.
 * @param flags Flags for the new namespace member.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOMEM`: Out of memory.
 */
uint64_t namespace_member_init(namespace_member_t* member, namespace_member_t* source, namespace_member_flags_t flags);

/**
 * @brief Clear and deinitialize a namespace.
 *
 * @param ns The namespace to deinitialize.
 */
void namespace_member_deinit(namespace_member_t* member);

/**
 * @brief Clears a namespace member.
 *
 * @param member The namespace member to clear.
 */
void namespace_member_clear(namespace_member_t* member);

/**
 * @brief If the given path is a mountpoint in the namespace, traverse to the mounted filesystem, else no-op.
 *
 * @param member The namespace member containing the namespace to traverse.
 * @param path The mountpoint path to traverse, will be updated to the new path if traversed.
 */
void namespace_traverse(namespace_member_t* member, path_t* path);

/**
 * @brief Mount a filesystem in a namespace.
 *
 * @param member The namespace member containing the namespace to mount to.
 * @param deviceName The device name, or `VFS_DEVICE_NAME_NONE` for no device.
 * @param target The target path to mount to.
 * @param fsName The filesystem name.
 * @param flags Mount flags.
 * @param mode The maximum allowed permissions for files/directories opened under this mount.
 * @param private Private data for the filesystem's mount function.
 * @return On success, the new mount. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EIO`: The filesystem returned a invalid root dentry.
 * - `EXDEV`: The target path is not visible in the namespace.
 * - `ENODEV`: The specified filesystem does not exist.
 * - `EBUSY`: Attempt to mount to already existing root.
 * - `ENOMEM`: Out of memory.
 * - `ENOENT`: The root does not exist or the target is negative.
 * - Other errors as returned by the filesystem's `mount()` function or `mount_new()`.
 */
mount_t* namespace_mount(namespace_member_t* member, path_t* target, const char* deviceName, const char* fsName,
    mount_flags_t flags, mode_t mode, void* private);

/**
 * @brief Bind a source dentry to a target path in a namespace.
 *
 * @param member The namespace member containing the namespace to bind in.
 * @param source The source dentry to bind from, could be either a file or directory and from any filesystem.
 * @param target The target path to bind to.
 * @param flags Mount flags.
 * @param mode The maximum allowed permissions for files/directories opened under this mount.
 * @return On success, the new mount. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOMEM`: Out of memory.
 * - Other errors as returned by `mount_new()`.
 */
mount_t* namespace_bind(namespace_member_t* member, dentry_t* source, path_t* target, mount_flags_t flags, mode_t mode);

/**
 * @brief Remove a mount in a namespace.
 *
 * @param member The namespace member containing the namespace to unmount from.
 * @param mount The mount to remove.
 * @param flags Mount flags.
 */
void namespace_unmount(namespace_member_t* member, mount_t* mount, mount_flags_t flags);

/**
 * @brief Get the root path of a namespace.
 *
 * @param member The namespace member containing the namespace to get the root of.
 * @param out The output root path.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The namespace has no root mount.
 */
uint64_t namespace_get_root_path(namespace_member_t* member, path_t* out);

/** @} */
