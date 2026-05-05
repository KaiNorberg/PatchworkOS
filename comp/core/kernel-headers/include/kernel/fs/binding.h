#pragma once

#include <kernel/fs/path.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/ref.h>

#include <libc/list.h>
#include <libc/map.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct binding binding_t;
typedef struct dentry dentry_t;

/**
 * @brief Binding structure.
 * @defgroup kernel_fs_binding Binding
 * @ingroup kernel_fs
 *
 * A binding represents a link between two locations within the filesystem hierarchy, causing one location to appear at
 * the other.
 *
 * @{
 */

/**
 * @brief Check if a binding is a root binding within its namespace.
 *
 * @param binding The binding to check.
 * @return `true` if the binding is a root binding, `false` otherwise.
 */
#define BINDING_IS_ROOT(binding) ((binding)->parent == NULL)

/**
 * @brief Binding ID type.
 */
typedef uint64_t binding_id_t;

/**
 * @brief Binding structure.
 * @struct binding_t
 *
 * Analogous to a "vfsmount" in Linux.
 */
typedef struct binding
{
    ref_t ref;
    binding_id_t id;
    dentry_t* source;  ///< The dentry to appear at the target, usually the root of the bound filesystem.
    dentry_t* target;  ///< The dentry which the source is bound to, can be `NULL` for the root filesystem.
    binding_t* parent; ///< The parent binding, can be `NULL` for the root filesystem.
    path_mode_t mode;  ///< Specifies the maximum permissions for this binding.
    rcu_entry_t rcu;   ///< RCU entry for deferred cleanup.
} binding_t;

/**
 * @brief Create a new binding.
 *
 * There is no `binding_free()` instead use `UNREF()`.
 *
 * @param source The dentry to appear at target once bound.
 * @param target The dentry which the source is bound to, can be `NULL` for a disconnected hierarchy.
 * @param parent The parent binding, can be `NULL` for a disconnected hierarchy.
 * @param mode Specifies the maximum permissions for files within this binding.
 * @return On success, the new binding. On failure, returns `NULL`.
 */
binding_t* binding_new(dentry_t* source, dentry_t* target, binding_t* parent, path_mode_t mode);

/** @} */
