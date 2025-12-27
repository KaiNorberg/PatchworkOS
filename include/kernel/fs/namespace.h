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
 * @{
 */

/**
 * @brief Maximum number of iterative mount traversals when following mountpoints.
 */
#define NAMESPACE_MAX_TRAVERSE 32

/**
 * @brief Mount stack entry.
 * @struct mount_stack_entry_t
 *
 * Used to store mounts in a stack, allowing the same mount to be stored in multiple namespaces.
 */
typedef struct mount_stack_entry
{
    list_entry_t entry;
    mount_t* mount;
} mount_stack_entry_t;

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
    list_t mounts; ///< List of `mount_stack_entry_t`.
} mount_stack_t;

/**
 * @brief Namespace handle flags.
 * @enum namespace_handle_flags_t
 */
typedef enum
{
    NAMESPACE_HANDLE_SHARE = 0 << 0, ///< Share the same namespace as the source.
    NAMESPACE_HANDLE_COPY =
        1 << 1, ///< Copy the contents of the source into a new namespace with the source as the parent.
    NAMESPACE_HANDLE_EMPTY =
        1 << 2, ///< Create a new empty namespace with the source as the parent.
} namespace_handle_flags_t;

/**
 * @brief Per-process namespace handle.
 * @struct namespace_handle_t
 *
 * Stored in each process, used to allow multiple processes to share a namespace.
 */
typedef struct namespace_handle
{
    list_entry_t entry;
    namespace_t* ns;
    rwlock_t lock;
} namespace_handle_t;

/**
 * @brief Namespace structure.
 * @struct namespace_t
 */
typedef struct namespace
{
    list_entry_t entry;  ///< The entry for the parent's children list.
    list_t children;     ///< List of child namespaces.
    namespace_t* parent; ///< The parent namespace, can be `NULL`.
    list_t stacks;       ///< List of stacks in this namespace.
    map_t mountMap;      ///< Map used to go from source dentries to namespace mount stacks.
    list_t handles;      ///< List of `namespace_handle_t` containing this namespace.
    rwlock_t lock;
    // clang-format off
} namespace_t;
// clang-format on

/**
 * @brief Initializes a namespace handle.
 *
 * @param handle The namespace handle to initialize.
 * @param source The source namespace handle to copy from or share a namespace with, or `NULL` to create a new empty
 * namespace.
 * @param flags Flags for the new namespace handle.
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENOMEM`: Out of memory.
 */
uint64_t namespace_handle_init(namespace_handle_t* handle, namespace_handle_t* source, namespace_handle_flags_t flags);

/**
 * @brief Clear and deinitialize a namespace.
 *
 * @param ns The namespace to deinitialize.
 */
void namespace_handle_deinit(namespace_handle_t* handle);

/**
 * @brief Clears a namespace handle.
 *
 * @param handle The namespace handle to clear.
 */
void namespace_handle_clear(namespace_handle_t* handle);

/**
 * @brief If the given path is a mountpoint in the namespace, traverse to the mounted filesystem, else no-op.
 *
 * @param handle The namespace handle containing the namespace to traverse.
 * @param path The mountpoint path to traverse, will be updated to the new path if traversed.
 * @return `true` if the path was modified, `false` otherwise.
 */
bool namespace_traverse(namespace_handle_t* handle, path_t* path);

/**
 * @brief Mount a filesystem in a namespace.
 *
 * @param handle The namespace handle containing the namespace to mount to.
 * @param deviceName The device name, or `VFS_DEVICE_NAME_NONE` for no device.
 * @param target The target path to mount to, can be `NULL` to mount to root.
 * @param fsName The filesystem name.
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
mount_t* namespace_mount(namespace_handle_t* handle, path_t* target, const char* deviceName, const char* fsName,
    mode_t mode, void* private);

/**
 * @brief Bind a source path to a target path in a namespace.
 *
 * @param handle The namespace handle containing the namespace to bind in.
 * @param source The source path to bind from, could be either a file or directory and from any filesystem.
 * @param target The target path to bind to, can be `NULL` to bind to root.
 * @param mode The mode specifying permissions and mount behaviour.
 * @return On success, the new mount. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EACCES`: The requested mode exceeds the maximum allowed permissions.
 * - `ENOMEM`: Out of memory.
 * - Other errors as returned by `mount_new()`.
 */
mount_t* namespace_bind(namespace_handle_t* handle, path_t* source, path_t* target, mode_t mode);

/**
 * @brief Remove a mount in a namespace.
 *
 * @param handle The namespace handle containing the namespace to unmount from.
 * @param mount The mount to remove.
 * @param mode The mode specifying unmount behaviour.
 */
void namespace_unmount(namespace_handle_t* handle, mount_t* mount, mode_t mode);

/**
 * @brief Get the root path of a namespace.
 *
 * @param handle The namespace handle containing the namespace to get the root of.
 * @param out The output root path, may be a invalid `NULL` path if the namespace is empty.
 */
void namespace_get_root(namespace_handle_t* handle, path_t* out);

/** @} */
