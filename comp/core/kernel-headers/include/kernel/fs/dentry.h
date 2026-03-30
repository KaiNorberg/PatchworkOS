#pragma once

#include <kernel/fs/path.h>
#include <kernel/fs/vnode.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rcu.h>
#include <kernel/sync/seqlock.h>
#include <kernel/utils/ref.h>

#include <libc/dstr.h>
#include <libc/fs.h>
#include <libc/list.h>
#include <libc/map.h>
#include <stdatomic.h>
#include <stdint.h>

typedef struct dentry dentry_t;
typedef struct vnode vnode_t;

/**
 * @brief Directory entry.
 * @defgroup kernel_fs_dentry Dentry
 * @ingroup kernel_fs
 *
 * A dentry respresents the name of a file within the VFS hierarchy and is used for path traversal.
 *
 * It acts as a link between the name and a vnode, allowing a vnode to appear in multiple places within the heirarchy.
 * The dentry itself can also appear in multiple places within the hierarchy due to mountpoints.
 *
 * @note While our dentries share a name with, and are similar to, Linux dentries, they do differ in some key ways. Most
 * notably, they are immutable after being made positive and out handling of negative dentries is simplified. Both these
 * changes where made for performance.
 *
 * ## Negative Dentries
 *
 * A negative dentry is a dentry that does not have an associated vnode. These are primarily used while creating files,
 * acting as a placeholder.
 *
 * Negative dentries are not cached for subsequent lookups.
 *
 * ## Immutability
 *
 * Dentries are immutable after being made positive. This means that once a dentry is associated with a vnode, its name,
 * parent, and vnode will never change. This allows lookups and path traversal to be almost lockless and more efficient
 * as there is no need for even a sequence lock.
 *
 * @{
 */

/**
 * @brief Dentry ID type.
 */
typedef uint64_t dentry_id_t;

/**
 * @brief Check if a dentry is positive.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is positive, false if it is negative.
 */
#define DENTRY_IS_POSITIVE(dentry) (dentry != NULL && (dentry)->vnode != NULL)

/**
 * @brief Check if a dentry is of a specific type.
 *
 * @param _dentry The dentry to check.
 * @param _type The type to check against.
 * @return `true` if the dentry is of the specified type, `false` otherwise.
 */
#define DENTRY_IS_TYPE(_dentry, _type) (DENTRY_IS_POSITIVE(_dentry) && (_dentry)->vnode->cls->type == (_type))

/**
 * @brief Directory entry structure.
 * @struct dentry_t
 *
 * A dentry structure is protected by the mutex of its vnode. Note that since move and rename are not supported in favor
 * of link and remove, the parent of a dentry will never change after creation which allows some optimizations.
 */
typedef struct dentry
{
    ref_t ref;
    dentry_id_t id;
    dstr_t name;      ///< The name of the dentry, immutable after creation.
    vnode_t* vnode;   ///< Will be `NULL` if the dentry is negative, once positive it will never be modified.
    dentry_t* parent; ///< The parent dentry, can be `NULL`, immutable after creation.
    void* data;       ///< Private data to store in the dentry, can be `NULL`.
    list_entry_t siblingEntry;
    list_t children;
    map_entry_t mapEntry;       ///< Entry in the dentry cache hash map.
    _Atomic(uint64_t) bindings; ///< Number of bindings targeting this dentry.
    rcu_entry_t rcu;            ///< RCU entry for deferred cleanup.
    list_entry_t entry;         ///< Made available for use by any other subsystems for convenience.
} dentry_t;

/**
 * @brief Allocate a new dentry.
 *
 * There is no `dentry_free()` instead use `UNREF()`.
 *
 * @param parent The parent dentry, can be `NULL`.
 * @param name The name of the dentry, can be `NULL` if `parent` is also `NULL`.
 * @param length The length of the name.
 * @return On success, the new negative dentry. On failure, returns `NULL`.
 */
dentry_t* dentry_new(dentry_t* parent, const char* name, size_t length);

/**
 * @brief Remove a dentry from the dentry cache.
 *
 * @note Will not free the dentry, use `UNREF()` for that.
 *
 * @param dentry The dentry to remove.
 */
void dentry_remove(dentry_t* dentry);

/**
 * @brief Get a dentry from the dentry cache in an RCU read-side critical section without traversing mountpoints.
 *
 * Will only check the dentry cache and return a dentry if it exists there, will not call the filesystem's lookup
 * function.
 *
 * @warning Will NOT return a reference to the dentry, the caller must ensure that this function is called in a RCU read
 * critical section.
 *
 * @param parent The parent path.
 * @param name The name of the dentry.
 * @param length The length of the name.
 * @return On success, the dentry, might be negative. On failure, returns `NULL`.
 */
dentry_t* dentry_rcu_get(const dentry_t* parent, const char* name, size_t length);

/**
 * @brief Ger a dentry from the dentry cache.
 *
 * Will only check the dentry cache and return a dentry if it exists there, will not call the filesystem's lookup
 * function.
 *
 * @param parent The parent path.
 * @param name The name of the dentry.
 * @param length The length of the name.
 * @return On success, the dentry, might be negative. On failure, returns `NULL`.
 */
dentry_t* dentry_get(const dentry_t* parent, const char* name, size_t length);

/**
 * @brief Make a dentry positive by associating it with an vnode.
 *
 * This function is expected to be protected by the parent vnode's mutex.
 *
 * @param dentry The dentry to make positive, or `NULL` for no-op.
 * @param vnode The vnode to associate with the dentry, or `NULL` for no-op.
 */
void dentry_make_positive(dentry_t* dentry, vnode_t* vnode);

/** @} */
