#pragma once

#include "fs/path.h"
#include "sync/mutex.h"
#include "utils/map.h"
#include "utils/ref.h"

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
 * associated inode, or negative, meaning it does not have an associated inode. When traversing a filesystem this is the
 * thing you actually walk through.
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
#define DENTRY_IS_ROOT(dentry) (dentry->parent == dentry)

/**
 * @brief Flags for a dentry.
 * @enum dentry_flags_t
 */
typedef enum
{
    DENTRY_NONE = 0, ///< No flags.
    /**
     * This dentry is negative, meaning it does not have an associated inode.
     *
     * A negative dentry is created when a lookup for a name in a directory fails, it is used to cache the fact that
     * the name does not exist in that directory and also allows us to avoid race conditions where two processes try to
     * create the same file at the same time.
     */
    DENTRY_NEGATIVE = 1 << 1,
} dentry_flags_t;

/**
 * @brief Dentry operations structure.
 * @struct dentry_ops_t
 *
 * Note that the dentrys mutex will be acquired by the vfs and any implemented operations do not need to acquire it
 * again.
 */
typedef struct dentry_ops
{
    /**
     * @brief Used to now what is in a directory.
     */
    uint64_t (*getdents)(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset, path_flags_t flags);
    void (*cleanup)(dentry_t* entry); ///< Called when the dentry is being freed.
} dentry_ops_t;

/**
 * @brief Directory entry structure.
 * @struct dentry_t
 */
typedef struct dentry
{
    ref_t ref;
    dentry_id_t id;
    char name[MAX_NAME];
    inode_t* inode;
    dentry_t* parent;
    list_entry_t siblingEntry;
    list_t children;
    mutex_t childrenMutex;
    superblock_t* superblock;
    const dentry_ops_t* ops;
    void* private;
    map_entry_t mapEntry;
    _Atomic(dentry_flags_t) flags;
    /**
     * The number of mounts on this dentry.
     *
     * If greater then 0 and a mountpoint mounted here is in a accessible namespace, we can traverse from this dentry to
     * the root dentry of the mounted filesystem.
     *
     * The difference between a mountpoint dentry and a root dentry can be a bit confusing, so here is a quick
     * explanation. When a filesystem is mounted the dentry that it gets mounted to becomes a mountpoint, any data that
     * was there before becomes hidden and when we traverse to that dentry we "jump" over to the root dentry of the
     * mounted filesystem. The root dentry of the mounted filesystem is simply the root directory of that filesystem.
     *
     * This means that the mountpoint does not "become" the root of the mounted filesystem, it simply points to it.
     *
     * Finally, note that just becouse a dentry is a mountpoint does not mean that it can be traversed by the current
     * process, a process can only traverse a mountpoint if it is visible in its namespace, if its not visible the
     * dentry acts exactly like a normal dentry.
     */
    atomic_uint64_t mountCount;
} dentry_t;

/**
 * @brief Create a new dentry.
 *
 * This does not add the dentry to the dentry cache, that must be done separately with `vfs_add_dentry()`. It also does
 * not add the dentry to its parent's list of children, that is done when the dentry is made positive with
 * `dentry_make_positive()`.
 *
 * There is no `dentry_free()` instead use `DEREF()`.
 *
 * @param superblock The superblock the dentry belongs to.
 * @param parent The parent dentry, can be NULL if this is a root dentry.
 * @param name The name of the dentry.
 * @return On success, the new dentry. On failure, returns `NULL` and `errno` is set.
 */
dentry_t* dentry_new(superblock_t* superblock, dentry_t* parent, const char* name);

/**
 * @brief Make a dentry positive by associating it with an inode.
 *
 * This will also add the dentry to its parent's list of children and call `vfs_add_dentry()` to add it to the dentry
 * cache.
 *
 * @param dentry The dentry to make positive.
 * @param inode The inode to associate with the dentry.
 */
uint64_t dentry_make_positive(dentry_t* dentry, inode_t* inode);

/**
 * @brief Increments the mount count of a dentry.
 *
 * @param dentry The dentry to increment the mount count of.
 */
void dentry_inc_mount_count(dentry_t* dentry);

/**
 * @brief Decrements the mount count of a dentry.
 *
 * @param dentry The dentry to decrement the mount count of.
 */
void dentry_dec_mount_count(dentry_t* dentry);

/**
 * @brief Helper function for a basic getdents.
 *
 * This function can be used by filesystems that do not have any special requirements for getdents, in practice ram
 * disks.
 */
uint64_t dentry_generic_getdents(dentry_t* dentry, dirent_t* buffer, uint64_t count, uint64_t* offset,
    path_flags_t flags);

/** @} */
