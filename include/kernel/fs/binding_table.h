#pragma once

#include <kernel/fs/path.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/ref.h>

#include <stdint.h>
#include <sys/fs.h>
#include <sys/list.h>
#include <sys/map.h>
#include <sys/status.h>

typedef struct binding_table binding_table_t;
typedef struct binding binding_t;
typedef struct process process_t;
typedef struct dentry dentry_t;

/**
 * @brief Binding Tables.
 * @defgroup kernel_fs_binding_table Binding Tables
 * @ingroup kernel_fs
 *
 * The per-file binding table system allows each root file specified while walking a path to have its own view of the
 * filesystem hierarchy.
 *
 * @{
 */

/**
 * @brief Maximum number of iterative binding traversals when following bindings.
 */
#define BINDING_TABLE_MAX_TRAVERSE 32

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
 * @brief Size of the binding map hash table.
 *
 * @note This size was choosen such that the size of a `file_t` is a power of two.
 */
#define BINDING_MAP_SIZE 49

/**
 * @brief Binding table structure.
 * @struct binding_table_t
 */
typedef struct binding_table
{
    list_t stacks;                            ///< List of binding stacks for fast iteration.
    MAP_DEFINE(bindingMap, BINDING_MAP_SIZE); ///< Map used to go from source dentries to binding stacks.
    rwlock_t lock;
} binding_table_t;

/**
 * @brief Initialize a binding table.
 *
 * @param table The binding table to initialize.
 */
void binding_table_init(binding_table_t* table);

/**
 * @brief Deinitialize a binding table.
 *
 * @param table The binding table to deinitialize.
 */
void binding_table_deinit(binding_table_t* table);

/**
 * @brief Copy bindings from one table to another.
 *
 * @param dest The destination table.
 * @param src The source table.
 * @return An appropriate status value.
 */
status_t binding_table_copy(binding_table_t* dest, binding_table_t* src);

/**
 * @brief If the given path has a binding in the table, traverse to the bound location in an RCU read critical
 * section, else no-op.
 *
 * @warning Will not increase the reference count of the returned path's binding and dentry, the caller must ensure that
 * they are not freed while in use.
 *
 * @param table The table containing the binding to traverse.
 * @param binding The output binding after traversal, may be unchanged if not traversed.
 * @param dentry The output dentry after traversal, may be unchanged if not traversed.
 * @return `true` if the path was modified, `false` otherwise.
 */
bool binding_table_rcu_traverse(binding_table_t* table, binding_t** binding, dentry_t** dentry);

/**
 * @brief Bind a source path to a target path in a table.
 *
 * @param table The table containing the binding to bind in.
 * @param target The target path to bind to.
 * @param source The source path to bind from, could be either a file or directory and from any filesystem.
 * @param mode The mode specifying permissions and binding behaviour.
 * @param out Output pointer to store the new binding, can be `NULL`.
 * @return An appropriate status value.
 */
status_t binding_table_bind(binding_table_t* table, path_t* target, path_t* source, mode_t mode, binding_t** out);

/**
 * @brief Remove a binding in a table.
 *
 * @param table The table containing the binding to unbinding from.
 * @param binding The binding to remove.
 */
void binding_table_unbind(binding_table_t* table, binding_t* binding);

/** @} */
