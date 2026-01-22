#pragma once

#include <kernel/fs/path.h>
#include <kernel/io/verb.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/map.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>
#include <stdint.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <time.h>

typedef struct vnode vnode_t;
typedef struct vnode_ops vnode_ops_t;
typedef struct superblock superblock_t;
typedef struct file_ops file_ops_t;
typedef struct dentry dentry_t;

/**
 * @brief Virtual node.
 * @defgroup kernel_fs_vnode Vnode
 * @ingroup kernel_fs
 *
 * A vnode represents the actual data and metadata of a file. It is referenced by dentries, which represent the name or
 * "location" of the file but a vnode can appear in multiple dentries due to hardlinks or mounts.
 *
 * @note Despite the name vnodes are in no way "nodes" in any kind of tree structure, that would be the dentries.
 *
 * ## Synchronization
 *
 * vnodes have an additional purpose within the Virtual File System (VFS) as they act as the primary means of
 * synchronization. All dentries synchronize upon their vnodes mutex, open files synchronize upon the mutex of the
 * underlying vnode and operations like create, remove, etc synchronize upon the vnode mutex of the parent directory.
 *
 * @todo Implement actually writing/syncing dirty vnodes, for now vnodes should use the notify functions but they will
 * never actually be "cleaned."
 *
 * @{
 */

/**
 * @brief vnode structure.
 * @struct vnode_t
 *
 * vnodes are owned by the filesystem, not the VFS.
 */
typedef struct vnode
{
    ref_t ref;
    vtype_t type;
    _Atomic(uint64_t) dentryCount; ///< The number of dentries pointing to this vnode.
    void* data; ///< Filesystem defined data.
    uint64_t size; ///< Used for convenience by certain filesystems, does not represent the file size.
    superblock_t* superblock;
    const vnode_ops_t* ops;
    const file_ops_t* fileOps;
    const verb_table_t* verbs;
    rcu_entry_t rcu;
    mutex_t mutex;
} vnode_t;

/**
 * @brief vnode operations structure.
 * @struct vnode_ops_t
 *
 * Note that the vnodes mutex will be acquired by the vfs.
 */
typedef struct vnode_ops
{
    /**
     * @brief Look up a dentry in a directory vnode.
     *
     * Should set the target dentry to be positive (give it an vnode), if the entry does not exist the operation
     * should still return success but leave the dentry negative.
     *
     * @param dir The directory vnode to look in.
     * @param target The dentry to look up.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*lookup)(vnode_t* dir, dentry_t* target);
    /**
     * @brief Handles both directories and files depending on mode.
     *
     * Takes in a negative dentry and creates the corresponding vnode to make the dentry positive.
     *
     * @param dir The directory vnode to create the entry in.
     * @param target The negative dentry to create.
     * @param mode The mode to create the entry with.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*create)(vnode_t* dir, dentry_t* target, mode_t mode);
    /**
     * @brief Set the vnode size to zero.
     *
     * @param target The vnode to truncate.
     */
    void (*truncate)(vnode_t* target);
    /**
     * @brief Make the same file vnode appear twice in the filesystem.
     *
     * @param dir The directory vnode to create the link in.
     * @param old The existing dentry containing the vnode to link to.
     * @param new The negative dentry to store the same vnode as old.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*link)(vnode_t* dir, dentry_t* old, dentry_t* new);
    /**
     * @brief Retrieve the path of the symbolic link.
     *
     * @param vnode The symbolic link vnode.
     * @param buffer The buffer to store the path in.
     * @param size The size of the buffer.
     * @return On success, the number of bytes read. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*readlink)(vnode_t* vnode, char* buffer, uint64_t size);
    /**
     * @brief Create a symbolic link.
     *
     * @param dir The directory vnode to create the symbolic link in.
     * @param target The negative dentry to create.
     * @param dest The path to which the symbolic link will point.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*symlink)(vnode_t* dir, dentry_t* target, const char* dest);
    /**
     * @brief Remove a file or directory.
     *
     * @param dir The directory vnode containing the target.
     * @param target The dentry to remove.
     * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
     */
    uint64_t (*remove)(vnode_t* dir, dentry_t* target);
    /**
     * @brief Cleanup function called when the vnode is being freed.
     *
     * @param vnode The vnode being freed.
     */
    void (*cleanup)(vnode_t* vnode);
} vnode_ops_t;

/**
 * @brief Create a new vnode.
 *
 * Does not associate the vnode with a dentry, that is done when a dentry is made positive with `dentry_make_positive()`.
 *
 * There is no `vnode_free()` instead use `UNREF()`.
 *
 * @param superblock The superblock the vnode belongs to.
 * @param type The vnode type.
 * @param ops The vnode operations.
 * @param fileOps The file operations for files opened on this vnode.
 * @return On success, the new vnode. On failure, returns `NULL` and `errno` is set.
 */
vnode_t* vnode_new(superblock_t* superblock, vtype_t type, const vnode_ops_t* ops,
    const file_ops_t* fileOps);

/**
 * @brief Truncate the vnode.
 *
 * The filesystem should implement the actual truncation in the vnode ops truncate function, this is just a helper to
 * call it.
 *
 * @param vnode The vnode to truncate.
 */
void vnode_truncate(vnode_t* vnode);

/** @} */
