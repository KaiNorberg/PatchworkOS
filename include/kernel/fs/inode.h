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
 * "location" of the file but a inode can appear in multiple dentries due to hardlinks or mounts.
 *
 * @note Despite the name inodes are in no way "nodes" in any kind of tree structure, that would be the dentries.
 *
 * ## Synchronization
 *
 * Inodes have an additional purpose within the Virtual File System (VFS) as they act as the primary means of
 * synchronization. All dentries synchronize upon their inodes mutex, open files synchronize upon the mutex of the
 * underlying inode and operations like create, remove, etc synchronize upon the inode mutex of the parent directory.
 *
 * @todo Implement actually writing/syncing dirty inodes, for now inodes should use the notify functions but they will
 * never actually be "cleaned."
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
    inode_number_t number; ///< A value that uniquely identifies the inode within its filesystem.
    inode_type_t type;
    inode_flags_t flags;
    _Atomic(uint64_t) dentryCount; ///< The number of dentries pointing to this inode.
    uint64_t size;
    uint64_t blocks;
    time_t accessTime; ///< Unix time stamp for the last inode access.
    time_t modifyTime; ///< Unix time stamp for last file content alteration.
    time_t changeTime; ///< Unix time stamp for the last file metadata alteration.
    time_t createTime; ///< Unix time stamp for the inode creation.
    void* private;
    superblock_t* superblock;
    const inode_ops_t* ops;
    const file_ops_t* fileOps;
    mutex_t mutex;
} inode_t;

/**
 * @brief Inode operations structure.
 * @struct inode_ops_t
 *
 * Note that the inodes mutex will be acquired by the vfs.
 */
typedef struct inode_ops
{
    /**
     * @brief Look up a dentry in a directory inode.
     *
     * Should set the target dentry to be positive (give it an inode), if the entry does not exist the operation
     * should still return success but leave the dentry negative.
     *
     * @param dir The directory inode to look in.
     * @param target The dentry to look up.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*lookup)(inode_t* dir, dentry_t* target);
    /**
     * @brief Handles both directories and files depending on mode.
     *
     * Takes in a negative dentry and creates the corresponding inode to make the dentry positive.
     *
     * @param dir The directory inode to create the entry in.
     * @param target The negative dentry to create.
     * @param mode The mode to create the entry with.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*create)(inode_t* dir, dentry_t* target, mode_t mode);
    /**
     * @brief Set the inode size to zero.
     *
     * @param target The inode to truncate.
     */
    void (*truncate)(inode_t* target);
    /**
     * @brief Make the same file inode appear twice in the filesystem.
     *
     * @param dir The directory inode to create the link in.
     * @param old The existing dentry containing the inode to link to.
     * @param new The negative dentry to store the same inode as old.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*link)(inode_t* dir, dentry_t* old, dentry_t* new);
    /**
     * @brief Retrieve the path of the symbolic link.
     *
     * @param inode The symbolic link inode.
     * @param buffer The buffer to store the path in.
     * @param size The size of the buffer.
     * @return On success, the number of bytes read. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*readlink)(inode_t* inode, char* buffer, uint64_t size);
    /**
     * @brief Create a symbolic link.
     *
     * @param dir The directory inode to create the symbolic link in.
     * @param target The negative dentry to create.
     * @param dest The path to which the symbolic link will point.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*symlink)(inode_t* dir, dentry_t* target, const char* dest);
    /**
     * @brief Remove a file or directory.
     *
     * @param dir The directory inode containing the target.
     * @param target The dentry to remove.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*remove)(inode_t* dir, dentry_t* target);
    /**
     * @brief Cleanup function called when the inode is being freed.
     *
     * @param inode The inode being freed.
     */
    void (*cleanup)(inode_t* inode);
} inode_ops_t;

/**
 * @brief Create a new inode.
 *
 * This DOES add the inode to the inode cache. It also does not associate the inode with a dentry, that is done when a
 * dentry is made positive with `dentry_make_positive()`.
 *
 * There is no `inode_free()` instead use `UNREF()`.
 *
 * @param superblock The superblock the inode belongs to.
 * @param number The inode number, for a generic filesystem `vfs_id_get()` can be used.
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
