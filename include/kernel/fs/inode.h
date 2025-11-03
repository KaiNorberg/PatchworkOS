#pragma once

#include <kernel/fs/path.h>
#include <kernel/sync/mutex.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>
#include <time.h>

// TODO: Implement actually writing/syncing dirty inodes, for now inodes should use the notify functions but they will
// never actually be "cleaned."

typedef struct inode inode_t;
typedef struct inode_ops inode_ops_t;
typedef struct superblock superblock_t;
typedef struct file_ops file_ops_t;
typedef struct dentry dentry_t;

/**
 * @brief Index node.
 * @defgroup kernel_fs_inode Inode
 * @ingroup kernel_fs
 *
 * A inode represents the actual data and metadata of a file. It is referenced by dentries, which represent the name or
 * "location" of the file but note that a inode can appear in multiple dentries due to hardlinks or becouse of
 * mountpoints.
 *
 * So despite the name they are in no way "nodes" in some kind of tree structure, this confused me for a
 * long time when first learning about filesystems.
 *
 * @{
 */

/**
 * @brief Inode flags.
 * @enum inode_flags_t
 */
typedef enum
{
    INODE_NONE = 0, ///< None
} inode_flags_t;

/**
 * @brief Inode structure.
 * @struct inode_t
 *
 * Inodes are owned by the filesystem, not the VFS.
 */
typedef struct inode
{
    ref_t ref;
    inode_number_t number; ///< Constant after creation.
    inode_type_t type;     ///< Constant after creation.
    inode_flags_t flags;
    uint64_t linkCount;
    uint64_t size;
    uint64_t blocks;
    time_t accessTime; ///< Unix time stamp for the last inode access.
    time_t modifyTime; ///< Unix time stamp for last file content alteration.
    time_t changeTime; ///< Unix time stamp for the last file metadata alteration.
    time_t createTime; ///< Unix time stamp for the inode creation.
    void* private;
    superblock_t* superblock;  ///< Constant after creation.
    const inode_ops_t* ops;    ///< Constant after creation.
    const file_ops_t* fileOps; ///< Constant after creation.
    mutex_t mutex;
    map_entry_t mapEntry; ///< Protected by the inodeCache lock.
} inode_t;

/**
 * Inode operations structure.
 *
 * Note that the inodes mutex will be acquired by the vfs.
 */
typedef struct inode_ops
{
    /**
     * @brief Should set the target dentry to be positive (give it an inode), if the entry does not exist the operation
     * should still return success and leave the dentry as negative, if other errors occur it should return ERR and set
     * errno.
     */
    uint64_t (*lookup)(inode_t* dir, dentry_t* target);
    /**
     * @brief Handles both directories and files, works the same as lookup.
     */
    uint64_t (*create)(inode_t* dir, dentry_t* target, path_flags_t flags);
    void (*truncate)(inode_t* target);
    uint64_t (*link)(dentry_t* old, inode_t* dir, dentry_t* target);
    /**
     * @brief Handles both directories and files.
     */
    uint64_t (*remove)(inode_t* parent, dentry_t* target, path_flags_t flags);
    void (*cleanup)(inode_t* inode);
} inode_ops_t;

/**
 * @brief Create a new inode.
 *
 * This DOES add the inode to the inode cache. It also does not associate the inode with a dentry, that is done when a
 * dentry is made positive with `dentry_make_positive()`.
 *
 * There is no `inode_free()` instead use `DEREF()`.
 *
 * @param superblock The superblock the inode belongs to.
 * @param number The inode number.
 * @param type The inode type.
 * @param ops The inode operations.
 * @param fileOps The file operations for files opened on this inode.
 * @return On success, the new inode. On failure, returns `NULL` and `errno` is set.
 */
inode_t* inode_new(superblock_t* superblock, inode_number_t number, inode_type_t type, const inode_ops_t* ops,
    const file_ops_t* fileOps);

/**
 * @brief Notify the inode that it has been accessed.
 *
 * This updates the access time.
 *
 * @param inode The inode to notify.
 */
void inode_notify_access(inode_t* inode);

/**
 * @brief Notify the inode that its content has been modified.
 *
 * This updates the modify time and change time.
 *
 * @param inode The inode to notify.
 */
void inode_notify_modify(inode_t* inode);

/**
 * @brief Notify the inode that its metadata has changed.
 *
 * This updates the change time.
 *
 * @param inode The inode to notify.
 */
void inode_notify_change(inode_t* inode);

/**
 * @brief Truncate the inode.
 *
 * The filesystem should implement the actual truncation in the inode ops truncate function, this is just a helper to
 * call it.
 *
 * @param inode The inode to truncate.
 */
void inode_truncate(inode_t* inode);

/** @} */
