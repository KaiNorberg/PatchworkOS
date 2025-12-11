#pragma once

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
 * @brief Dentry flags.
 * @enum dentry_flags_t
 */
typedef enum
{
    DENTRY_NEGATIVE = 1 << 0, ///< Dentry is negative (no associated inode).
} dentry_flags_t;

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
 * @brief Dentry operations structure.
 * @struct dentry_ops_t
 */
typedef struct dentry_ops
{
    uint64_t (*getdents)(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset, mode_t mode);
    void (*cleanup)(dentry_t* entry); ///< Called when the dentry is being freed.
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
    inode_t* inode;      ///< Will be `NULL` if the dentry is negative, once positive it will never be `NULL`.
    _Atomic(dentry_flags_t) flags;
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
 * @brief Check if a dentry is positive.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is positive, false if it is negative.
 */
bool dentry_is_positive(dentry_t* dentry);

/**
 * @brief Check if the inode associated with a dentry is a file.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is a file, false otherwise or if the dentry is negative.
 */
bool dentry_is_file(dentry_t* dentry);

/**
 * @brief Check if the inode associated with a dentry is a directory.
 *
 * @param dentry The dentry to check.
 * @return true if the dentry is a directory, false otherwise or if the dentry is negative.
 */
bool dentry_is_dir(dentry_t* dentry);

/**
 * @brief Helper function for a basic getdents.
 *
 * This function can be used by filesystems that do not have any special requirements for getdents.
 *
 * In practice this is only useful for in-memory filesystems.
 *
 * Used by setting the dentry ops getdents to this function.
 */
uint64_t dentry_generic_getdents(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset, mode_t mode);

/** @} */
