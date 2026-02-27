#pragma once

#include <kernel/fs/path.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/ref.h>

#include <stdint.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/map.h>
#include <sys/status.h>

typedef struct namespace namespace_t;
typedef struct binding binding_t;
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
 * @brief Maximum number of iterative binding traversals when following bindings.
 */
#define NAMESPACE_MAX_TRAVERSE 32

/**
 * @brief Maximum number of bindings that can be bound to a single location.
 */
#define BINDING_STACK_MAX_BINDINGS 8

/**
 * @brief Binding stack.
 * @struct binding_stack_t
 *
 * Used to store a stack of bindings for a single path. The last binding added to the stack is given priority.
 */
typedef struct binding_stack
{
    list_entry_t entry;
    map_entry_t mapEntry;
    uint64_t parentId;
    uint64_t locationId;
    binding_t* bindings[BINDING_STACK_MAX_BINDINGS];
    uint64_t count;
} binding_stack_t;

/**
 * @brief Namespace structure.
 * @struct namespace_t
 */
typedef struct namespace
{
    ref_t ref;
    list_entry_t entry;       ///< The entry for the parent's children list.
    list_t children;          ///< List of child namespaces.
    namespace_t* parent;      ///< The parent namespace, can be `NULL`.
    list_t stacks;            ///< List of `binding_stack_t` in this namespace.
    MAP_DEFINE(bindingMap, 64); ///< Map used to go from source dentries to namespace binding stacks.
    binding_stack_t root;       ///< The root binding stack.
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
 * @brief Copy bindings from one namespace to another.
 *
 * @param dest The destination namespace.
 * @param src The source namespace.
 * @return An appropriate status value.
 */
status_t namespace_copy(namespace_t* dest, namespace_t* src);

/**
 * @brief Check if bindings in a namespace can be propagated to another namespace.
 *
 * This is equivalent to checkin if `other` is a child of `handle` and is intended to be used for security checks.
 *
 * If `handle` stores the same namespace as `other`, this will also return `true`.
 *
 * @param ns The source namespace.
 * @param other The target namespace.
 * @return `true` if bindings can be propagated, `false` otherwise.
 */
bool namespace_accessible(namespace_t* ns, namespace_t* other);

/**
 * @brief If the given path has a binding in the namespace, traverse to the bound location in an RCU read critical
 * section, else no-op.
 *
 * @warning Will not increase the reference count of the returned path's binding and dentry, the caller must ensure that
 * they are not freed while in use.
 *
 * @param ns The namespace containing the namespace to traverse.
 * @param binding The output binding after traversal, may be unchanged if not traversed.
 * @param dentry The output dentry after traversal, may be unchanged if not traversed.
 * @return `true` if the path was modified, `false` otherwise.
 */
bool namespace_rcu_traverse(namespace_t* ns, binding_t** binding, dentry_t** dentry);

/**
 * @brief Bind a source path to a target path in a namespace.
 *
 * @param ns The namespace containing the namespace to bind in.
 * @param target The target path to bind to, can be `NULL` to bind to root.
 * @param source The source path to bind from, could be either a file or directory and from any filesystem.
 * @param mode The mode specifying permissions and binding behaviour.
 * @param out Output pointer to store the new binding, can be `NULL`.
 * @return An appropriate status value.
 */
status_t namespace_bind(namespace_t* ns, path_t* target, path_t* source, mode_t mode, binding_t** out);

/**
 * @brief Remove a binding in a namespace.
 *
 * @param ns The namespace containing the namespace to unbinding from.
 * @param binding The binding to remove.
 * @param mode The mode specifying unbinding behaviour.
 */
void namespace_unbind(namespace_t* ns, binding_t* binding, mode_t mode);

/**
 * @brief Get the root path of a namespace.
 *
 * @param ns The namespace containing the namespace to get the root of.
 * @param out The output root path, may be a invalid `NULL` path if the namespace is empty.
 */
void namespace_get_root(namespace_t* ns, path_t* out);

/**
 * @brief Get the root binding of a namespace in an RCU read critical section.
 *
 * @warning Will not increase the reference count of the returned binding, the caller must ensure that the binding is not
 * freed while in use.
 *
 * @param ns The namespace containing the namespace to get the root binding of.
 * @param binding The output root binding, may be `NULL` if the namespace is empty.
 * @param dentry The output root dentry, may be `NULL` if the namespace is empty.
 */
void namespace_rcu_get_root(namespace_t* ns, binding_t** binding, dentry_t** dentry);

/** @} */
