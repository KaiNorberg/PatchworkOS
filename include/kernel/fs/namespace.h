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
 * The per-process namespace system allows each process to have its own view of the filesystem hierarchy, by having each process store its own set of mountpoints instead of having a global set of mountpoints. 
 *
 * ## Propagation
 *
 * When a new mount or bind is created in a namespace, it is only added to that specific namespace. However, its possible to propagate mounts to children and/or parent namespaces using mount flags (`mount_flags_t`), this allows those namespaces to also see the new mount or bind.
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
} namespace_mount_t;

/**
 * @brief Namespace structure.
 * @struct namespace
 *
 * Stored in each process structure.
 */
typedef struct namespace
{
    list_entry_t entry;  ///< The entry for the parent's children list.
    list_t children;    ///< List of child namespaces.
    namespace_t* parent; ///< The parent namespace, can be `NULL`.
    list_t mounts; ///< List of mounts in this namespace.
    map_t mountMap;  ///< Stores the same mounts as `mounts` but in a map for fast lookup.
    mount_t* root; ///< The root mount of the namespace.
    rwlock_t lock;
    // clang-format off
} namespace_t;
// clang-format on

/**
 * @brief Initializes a namespace.
 *
 * @param ns The namespace to initialize.
 * @param parent The parent namespace to inherit all mounts from, can be `NULL` to create an empty namespace.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `ENOMEM`: Out of memory.
 */
uint64_t namespace_init(namespace_t* ns, namespace_t* parent);

/**
 * @brief Deinitializes a namespace.
 *
 * @note The parent of the namespace will inherit all child namespaces of the deinitialized namespace.
 * 
 * @param ns The namespace to deinitialize.
 */
void namespace_deinit(namespace_t* ns);

/**
 * @brief Traverse a mountpoint path to the root of the mounted filesystem.
 *
 * @param ns The namespace to use.
 * @param mountpoint The mountpoint path to traverse.
 * @param out The output path, if the mountpoint exists, the root of the mounted filesystem, otherwise will be set to `mountpoint`.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 */
uint64_t namespace_traverse_mount(namespace_t* ns, const path_t* mountpoint, path_t* out);

/**
 * @brief Mount a filesystem in a namespace.
 *
 * @param ns The namespace to mount in.
 * @param deviceName The device name, or `VFS_DEVICE_NAME_NONE` for no device.
 * @param mountpoint The mountpoint path.
 * @param fsName The filesystem name.
 * @param flags Mount flags.
 * @param private Private data for the filesystem's mount function.
 * @return On success, the new mount. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENODEV`: The specified filesystem does not exist.
 * - `EIO`: The root is negative, should never happen if the filesystem is implemented correctly.
 * - `EBUSY`: Attempt to mount to already existing root.
 * - `ENOMEM`: Out of memory.
 * - `ENOENT`: The root does not exist or the mountpoint is negative.
 * - Other errors as returned by the filesystem's `mount()` function.
 */
mount_t* namespace_mount(namespace_t* ns, path_t* mountpoint, const char* deviceName, const char* fsName, mount_flags_t flags,
    void* private);

/**
 * @brief Bind a directory to a mountpoint in a namespace.
 *
 * @param ns The namespace to mount in.
 * @param source The source directory to bind.
 * @param mountpoint The mountpoint path.
 * @param flags Mount flags.
 * @return On success, the new mount. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The source is negative.
 * - `ENOMEM`: Out of memory.
 */
mount_t* namespace_bind(namespace_t* ns, dentry_t* source, path_t* mountpoint, mount_flags_t flags);

/**
 * @brief Get the root path of a namespace.
 *
 * @param ns The namespace, can be `NULL` to get the kernel process's namespace root.
 * @param out The output root path.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOENT`: The namespace has no root mount.
 */
uint64_t namespace_get_root_path(namespace_t* ns, path_t* out);

/** @} */
