#pragma once

#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/ref.h>

#include <sys/status.h>
#include <stdint.h>
#include <sys/map.h>
#include <sys/fs.h>
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
    uint64_t parentId;
    uint64_t mountpointId;
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
    MAP_DEFINE(mountMap, 64); ///< Map used to go from source dentries to namespace mount stacks.
    mount_stack_t root;  ///< The root mount stack.
    rwlock_t lock;
    // clang-format off
} namespace_t;
// clang-format on

/**
 * @brief Create a new namespace.
 *
 * There is no `namespace_free()` instead use `UNREF()`.
 *
 * @param parent The parent namespace, or `NULL` to create a root namespace.
 * @return On success, the new namespace. On failure, `NULL`.
 */
namespace_t* namespace_new(namespace_t* parent);

/**
 * @brief Copy mounts from one namespace to another.
 *
 * @param dest The destination namespace.
 * @param src The source namespace.
 * @return An appropriate status value.
 */
status_t namespace_copy(namespace_t* dest, namespace_t* src);

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
 * @brief If the given path is a mountpoint in the namespace, traverse to the mounted filesystem in an RCU read critical
 * section, else no-op.
 *
 * @warning Will not increase the reference count of the returned path's mount and dentry, the caller must ensure that
 * they are not freed while in use.
 *
 * @param ns The namespace containing the namespace to traverse.
 * @param mount The output mount after traversal, may be unchanged if not traversed.
 * @param dentry The output dentry after traversal, may be unchanged if not traversed.
 * @return `true` if the path was modified, `false` otherwise.
 */
bool namespace_rcu_traverse(namespace_t* ns, mount_t** mount, dentry_t** dentry);

/**
 * @brief Mount a filesystem in a namespace.
 *
 * @param ns The namespace containing the namespace to mount to.
 * @param target The target path to mount to, can be `NULL` to mount to root.
 * @param fs The filesystem to mount.
 * @param options A string containing filesystem defined `key=value` pairs, with multiple options separated by commas,
 * or `NULL`.
 * @param flags Mount flags.
 * @param mode The mode specifying permissions and mount behaviour.
 * @param data Private data for the filesystem's mount function.
 * @param out Output pointer to store the new mount, can be `NULL`.
 * @return An appropriate status value.
 */
status_t namespace_mount(namespace_t* ns, path_t* target, filesystem_t* fs, const char* options, mode_t mode,
    void* data, mount_t** out);

/**
 * @brief Bind a source path to a target path in a namespace.
 *
 * @param ns The namespace containing the namespace to bind in.
 * @param target The target path to bind to, can be `NULL` to bind to root.
 * @param source The source path to bind from, could be either a file or directory and from any filesystem.
 * @param mode The mode specifying permissions and mount behaviour.
 * @param out Output pointer to store the new mount, can be `NULL`.
 * @return An appropriate status value.
 */
status_t namespace_bind(namespace_t* ns, path_t* target, path_t* source, mode_t mode, mount_t** out);

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

/**
 * @brief Get the root mount of a namespace in an RCU read critical section.
 *
 * @warning Will not increase the reference count of the returned mount, the caller must ensure that the mount is not
 * freed while in use.
 *
 * @param ns The namespace containing the namespace to get the root mount of.
 * @param mount The output root mount, may be `NULL` if the namespace is empty.
 * @param dentry The output root dentry, may be `NULL` if the namespace is empty.
 */
void namespace_rcu_get_root(namespace_t* ns, mount_t** mount, dentry_t** dentry);

/** @} */
