#pragma once

#include "path.h"
#include "superblock.h"
#include "sync/rwlock.h"
#include "utils/map.h"

typedef struct namespace namespace_t;
typedef struct mount mount_t;
typedef struct process process_t;

/**
 * @brief Per-process Namespaces.
 * @defgroup kernel_fs_namespace Namespaces
 * @ingroup kernel_fs
 *
 * The Per-process namespace system allows each process to have its own view of the filesystem hierarchy, where the
 * children of a parent can see all the mount points that its parents can but can also have additional mount points that
 * are only visible to itself and its children.
 *
 * For example, say that in the kernel process we define the `/usr` directory, and then a child process mounts a
 * filesystem at `/usr/local`. The kernel process and its other children will not see the `/usr/local` mount point, but
 * the child process that created it and its own children will see it and be able to access it.
 *
 * This also has the interesting side effect that its possible to hide directories from child processes by mounting a
 * filesystem on to the directory, causing the children to se the mounted filesystem and not the original directory.
 *
 * @{
 */

/**
 * @brief Namespace structure.
 * @struct namespace
 *
 * Stored in each process structure.
 */
typedef struct namespace
{
    map_t mountPoints;
    rwlock_t lock;
    namespace_t* parent;
    process_t* owner;   ///< The process that owns this namespace, will not take a reference.
    mount_t* rootMount; ///< The root mount of the namespace, will be inherited from the parent namespace.
    // clang-format off
} namespace_t;
// clang-format on

/**
 * @brief Initializes a namespace.
 *
 * @param ns The namespace to initialize.
 * @param parent The parent namespace, can be `NULL`.
 * @param owner The process that owns this namespace.
 * @return On success, 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t namespace_init(namespace_t* ns, namespace_t* parent, process_t* owner);

/**
 * @brief Deinitializes a namespace.
 *
 * @param ns The namespace to deinitialize.
 */
void namespace_deinit(namespace_t* ns);

/**
 * @brief Traverse a mountpoint path to the root of the mounted filesystem.
 *
 * If the mount point is not found in the namespace or its parents, it will simply return the same path as the
 * mountpoint.
 *
 * @param outRoot The output root path.
 * @param ns The namespace to use.
 * @param mountpoint The mountpoint path to traverse.
 * @return On success, 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t namespace_traverse_mount(namespace_t* ns, const path_t* mountpoint, path_t* outRoot);

/**
 * @brief Mount a filesystem in a namespace.
 *
 * If `ns` is `NULL`, the filesystem will be mounted in the kernel process's namespace which will make it visible to all
 * processes.
 *
 * @param ns The namespace to mount in, can be `NULL`.
 * @param deviceName The device name, or `VFS_DEVICE_NAME_NONE` for no device.
 * @param mountpoint The mountpoint path.
 * @param fsName The filesystem name.
 * @param flags Superblock flags.
 * @param outRoot The output root path of the mounted filesystem, can be `NULL`.
 * @param private Private data for the filesystem's mount function.
 * @return On success, 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t namespace_mount(namespace_t* ns, path_t* mountpoint, const char* deviceName, const char* fsName,
    path_t* outRoot, void* private);

/**
 * @brief Get the root path of a namespace.
 *
 * @param ns The namespace, can be `NULL` to get the kernel process's namespace root.
 * @param outPath The output root path.
 * @return On success, 0. On failure, returns `ERR` and `errno` is set.
 */
uint64_t namespace_get_root_path(namespace_t* ns, path_t* outPath);

/** @} */
