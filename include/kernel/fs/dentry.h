#pragma once

#include <kernel/fs/inode.h>
#include <kernel/fs/path.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/seqlock.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct dentry dentry_t;
typedef struct dentry_ops dentry_ops_t;
typedef struct inode inode_t;
typedef struct superblock superblock_t;
typedef struct dir_ctx dir_ctx_t;

/**
 * @brief Directory entry.
 * @defgroup kernel_fs_dentry Dentry
 * @ingroup kernel_fs
 *
 * A dentry represents the actual name in the filesystem hierarchy. It can be either positive, meaning it has an
 * associated inode, or negative, meaning it does not have an associated inode.
 *
 * ## Mountpoints and Root Dentries
 *
 * The difference between a mountpoint dentry and a root dentry can be a bit confusing, so here is a quick
 * explanation. When a filesystem is mounted the dentry that it gets mounted to becomes a mountpoint, any data that
 * was there before becomes hidden and when we traverse to that dentry we "jump" to the root dentry of the
 * mounted filesystem. The root dentry of the mounted filesystem is simply the root directory of that filesystem.
 *
 * This means that the mountpoint does not "become" the root of the mounted filesystem, it simply points to it.
 *
 * Finally, note that just becouse a dentry is a mountpoint does not mean that it can be traversed by the current
 * process, a process can only traverse a mountpoint if it is visible in its namespace, if its not visible the
 * dentry acts exactly like a normal dentry.
 *
 * @{
 */

/**
 * @brief Dentry ID type.
 */
typedef uint64_t dentry_id_t;

/**
 * @brief Macro to check if a dentry is the root entry in its filesystem.
 *
 * A dentry is considered the root if its parent is itself.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is the root, false otherwise.
 */
#define DENTRY_IS_ROOT(dentry) ((dentry)->parent == (dentry))

/**
 * @brief Check if a dentry is positive.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is positive, false if it is negative.
 */
#define DENTRY_IS_POSITIVE(dentry) ((dentry)->inode != NULL)

/**
 * @brief Check if the inode associated with a dentry is a file.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is a file, false otherwise or if the dentry is negative.
 */
#define DENTRY_IS_FILE(dentry) (DENTRY_IS_POSITIVE(dentry) && (dentry)->inode->type == INODE_FILE)

/**
 * @brief Check if the inode associated with a dentry is a directory.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is a directory, false otherwise or if the dentry is negative.
 */
#define DENTRY_IS_DIR(dentry) (DENTRY_IS_POSITIVE(dentry) && (dentry)->inode->type == INODE_DIR)

/**
 * @brief Check if the inode associated with a dentry is a symbolic link.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is a symbolic link, false otherwise or if the dentry is negative.
 */
#define DENTRY_IS_SYMLINK(dentry) (DENTRY_IS_POSITIVE(dentry) && (dentry)->inode->type == INODE_SYMLINK)

/**
 * @brief Directory context used to iterate over directory entries.
 */
typedef struct dir_ctx
{
    /**
     * @brief Emit function.
     *
     * Should be called on all entries inside a directory while iterating over it, until this function returns `false`.
     *
     * Will be implemented by the VFS not the filesystem.
     *
     * @param ctx The directory context.
     * @param name The name of the entry.
     * @param number The inode number of the entry.
     * @param type The inode type of the entry.
     * @return `true` to continue iterating, `false` to stop.
     */
    bool (*emit)(dir_ctx_t* ctx, const char* name, inode_number_t number, inode_type_t type);
    uint64_t pos; ///< The current position in the directory, can be used to skip entries.
    void* private; ///< Private data that the filesystem can use to conveniently pass data.
    uint64_t index; ///< An index that the filesystem can use for its own purposes.
} dir_ctx_t;

/**
 * @brief Dentry operations structure.
 * @struct dentry_ops_t
 */
typedef struct dentry_ops
{
    /**
     * @brief Iterate over the entries in a directory dentry.
     *
     * @param dentry The directory dentry to iterate over.
     * @param ctx The directory context to use for iteration.
     * @return On success, `0`. On failure, `ERR` and `errno` is set.
     */
    uint64_t (*iterate)(dentry_t* dentry, dir_ctx_t* ctx);
    /**
     * @brief Called when the dentry is being freed.
     *
     * @param dentry The dentry being cleaned up.
     */
    void (*cleanup)(dentry_t* dentry);
} dentry_ops_t;

/**
 * @brief Directory entry structure.
 * @struct dentry_t
 *
 * A dentry structure is protected by the mutex of its inode.
 */
typedef struct dentry
{
    ref_t ref;
    dentry_id_t id;
    char name[MAX_NAME]; ///< Constant after creation.
    inode_t* inode;      ///< Will be `NULL` if the dentry is negative, once positive it will never be modified.
    dentry_t* parent;
    list_entry_t siblingEntry;
    list_t children;
    superblock_t* superblock;
    const dentry_ops_t* ops;
    void* private;
    map_entry_t mapEntry;
    _Atomic(uint64_t) mountCount; ///< Number of mounts targeting this dentry.
    list_entry_t otherEntry;      ///< Made available for use by any other subsystems for convenience.
} dentry_t;

/**
 * @brief Create a new dentry.
 *
 * Will not add the dentry to its parent's list of children but it will appear in the dentry cache as a negative dentry
 * until `dentry_make_positive()` is called making it positive. This is needed to solve some race conditions when
 * creating new files. While the dentry is negative it is not possible to create another dentry of the same name in the
 * same parent, and any lookup to the dentry will fail until it is made positive.
 *
 * There is no `dentry_free()` instead use `UNREF()`.
 *
 * @param superblock The superblock the dentry belongs to.
 * @param parent The parent dentry, can be NULL if this is a root dentry.
 * @param name The name of the dentry.
 * @return On success, the new dentry. On failure, returns `NULL` and `errno` is set.
 */
dentry_t* dentry_new(superblock_t* superblock, dentry_t* parent, const char* name);

/**
 * @brief Remove a dentry from the filesystem hierarchy.
 *
 * @note Will not free the dentry, use `UNREF()` for that.
 *
 * @param dentry The dentry to remove.
 */
void dentry_remove(dentry_t* dentry);

/**
 * @brief Get a dentry for the given name. Will NOT traverse mountpoints.
 *
 * Will only check the dentry cache and return a dentry if it exists there, will not call the filesystem's lookup
 * function.
 *
 * @param parent The parent path.
 * @param name The name of the dentry.
 * @return On success, the dentry, might be negative. On failure, returns `NULL` and `errno` is set.
 */
dentry_t* dentry_get(const dentry_t* parent, const char* name);

/**
 * @brief Lookup a dentry for the given name. Will NOT traverse mountpoints.
 *
 * If the dentry is not found in the dentry cache, the filesystem's lookup function will be called to try to find it.
 *
 * @param parent The parent path.
 * @param name The name of the dentry.
 * @return On success, the dentry, might be negative. On failure, returns `NULL` and `errno` is set.
 */
dentry_t* dentry_lookup(const path_t* parent, const char* name);

/**
 * @brief Make a dentry positive by associating it with an inode.
 *
 * This function is expected to be protected by the parent inode's mutex.
 *
 * @param dentry The dentry to make positive, or `NULL` for no-op.
 * @param inode The inode to associate with the dentry, or `NULL` for no-op.
 */
void dentry_make_positive(dentry_t* dentry, inode_t* inode);

/**
 * @brief The amount of special entries "." and ".." that `dentry_iterate_dots()` emits.
 */
#define DENTRY_DOTS_AMOUNT 2

/**
 * @brief Helper function to iterate over the special entries "." and "..".
 * 
 * Intended to be used in filesystem iterate implementations.
 *
 * @param dentry The directory dentry to iterate over.
 * @param ctx The directory context to use for iteration.
 * @return `true` if the iteration should continue, `false` if it should stop.
 */
bool dentry_iterate_dots(dentry_t* dentry, dir_ctx_t* ctx);

/**
 * @brief Helper function for a basic iterate.
 */
uint64_t dentry_generic_iterate(dentry_t* dentry, dir_ctx_t* ctx);

/** @} */
