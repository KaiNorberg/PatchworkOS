#pragma once

#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct namespace namespace_t;
typedef struct mount mount_t;
typedef struct process process_t;
typedef struct dentry dentry_t;

/**
 * @brief Per-process Namespaces.
 * @defgroup kernel_fs_namespace Namespaces
 * @ingroup kernel_fs
 *
 * The per-process namespace system allows each process to have its own view of the filesystem hierarchy, acting as the
 * primary form of security.
 *
 * @{
 */

/**
 * @brief Maximum number of iterative mount traversals when following mountpoints.
 */
#define NAMESPACE_MAX_TRAVERSE 32

/**
 * @brief Maximum number of mounts that can be mounted to a single mountpoint.
 */
#define MOUNT_STACK_MAX_MOUNTS 8

/**
 * @brief Mount stack.
 * @struct mount_stack_t
 *
 * Used to store a stack of mounts for a single path. The last mount added to the stack is given priority.
 */
typedef struct mount_stack
{
    list_entry_t entry;
    map_entry_t mapEntry;
    mount_t* mounts[MOUNT_STACK_MAX_MOUNTS];
    uint64_t count;
} mount_stack_t;

/**
 * @brief Namespace structure.
 * @struct namespace_t
 */
typedef struct namespace
{
    ref_t ref;
    list_entry_t entry;  ///< The entry for the parent's children list.
    list_t children;     ///< List of child namespaces.
    namespace_t* parent; ///< The parent namespace, can be `NULL`.
    list_t stacks;       ///< List of `mount_stack_t` in this namespace.
    map_t mountMap;      ///< Map used to go from source dentries to namespace mount stacks.
    rwlock_t lock;
    // clang-format off
} namespace_t;
// clang-format on

/**
 * @brief Create a new namespace.
 *
 * @param parent The parent namespace, or `NULL` to create a root namespace.
 * @return On success, the new namespace. On failure, `NULL` and `errno` is set to:
 * - `ENOMEM`: Out of memory.
 */
namespace_t* namespace_new(namespace_t* parent);

/**
 * @brief Copy mounts from one namespace to another.
 *
 * @param dest The destination namespace.
 * @param src The source namespace.
 * @return On success, `0`. On failure, `ERR` and `errno`
 */
uint64_t namespace_copy(namespace_t* dest, namespace_t* src);

/**
 * @brief Check if mounts in a namespace can be propagated to another namespace.
 *
 * This is equivalent to checkin if `other` is a child of `handle` and is intended to be used for security checks.
 *
 * If `handle` stores the same namespace as `other`, this will also return `true`.
 *
 * @param ns The source namespace.
 * @param other The target namespace.
 * @return `true` if mounts can be propagated, `false` otherwise.
 */
bool namespace_accessible(namespace_t* ns, namespace_t* other);

/**
 * @brief If the given path is a mountpoint in the namespace, traverse to the mounted filesystem, else no-op.
 *
 * @param ns The namespace containing the namespace to traverse.
 * @param path The mountpoint path to traverse, will be updated to the new path if traversed.
 * @return `true` if the path was modified, `false` otherwise.
 */
bool namespace_traverse(namespace_t* ns, path_t* path);

/**
 * @brief Mount a filesystem in a namespace.
 *
 * @param ns The namespace containing the namespace to mount to.
 * @param target The target path to mount to, can be `NULL` to mount to root.
 * @param fsName The filesystem name.
 * @param deviceName The device name, or `NULL` for no device.
 * @param flags Mount flags.
 * @param mode The mode specifying permissions and mount behaviour.
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
mount_t* namespace_mount(namespace_t* ns, path_t* target, const char* fsName, const char* deviceName, mode_t mode,
    void* private);

/**
 * @brief Bind a source path to a target path in a namespace.
 *
 * @param ns The namespace containing the namespace to bind in.
 * @param target The target path to bind to, can be `NULL` to bind to root.
 * @param source The source path to bind from, could be either a file or directory and from any filesystem.
 * @param mode The mode specifying permissions and mount behaviour.
 * @return On success, the new mount. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EACCES`: The requested mode exceeds the maximum allowed permissions.
 * - `ENOMEM`: Out of memory.
 * - Other errors as returned by `mount_new()`.
 */
mount_t* namespace_bind(namespace_t* ns, path_t* target, path_t* source, mode_t mode);

/**
 * @brief Remove a mount in a namespace.
 *
 * @param ns The namespace containing the namespace to unmount from.
 * @param mount The mount to remove.
 * @param mode The mode specifying unmount behaviour.
 */
void namespace_unmount(namespace_t* ns, mount_t* mount, mode_t mode);

/**
 * @brief Get the root path of a namespace.
 *
 * @param ns The namespace containing the namespace to get the root of.
 * @param out The output root path, may be a invalid `NULL` path if the namespace is empty.
 */
void namespace_get_root(namespace_t* ns, path_t* out);

/** @} */
