#pragma once

#include <kernel/fs/devfs.h>
#include <kernel/utils/ref.h>

#include <stdint.h>
#include <sys/io.h>
#include <sys/list.h>

typedef struct filesystem filesystem_t;
typedef struct superblock superblock_t;
typedef struct superblock_ops superblock_ops_t;
typedef struct dentry_ops dentry_ops_t;
typedef struct inode inode_t;
typedef struct dentry dentry_t;

/**
 * @brief Mounted filesystem.
 * @defgroup kernel_fs_superblock Superblock
 * @ingroup kernel_fs
 *
 * A superblock represents a mounted filesystem, it can be thought of as "filesystem + device". The filesystem is
 * just the format of the data, e.g. fat32, tmpfs, devfs, etc. and the device provides the data.
 *
 * In the case of certain special filesystems like tmpfs or devfs there is no physical device, a virtual device will be
 * specified (a device of type `0`).
 *
 * @{
 */

/**
 * @brief Superblock structure.
 * @struct superblock_t
 *
 * Superblocks are owned by the VFS, not the filesystem.
 */
typedef struct superblock
{
    ref_t ref;
    list_entry_t entry;
    dev_t device;
    uint64_t blockSize;
    uint64_t maxFileSize;
    void* private;
    dentry_t* root; ///< Root dentry of the filesystem, should not take a reference.
    const superblock_ops_t* ops;
    const dentry_ops_t* dentryOps;
    filesystem_t* fs;
    /**
     * The number of mounts of this superblock.
     *
     * Note that this does need to be separate from the reference count as a superblock is referenced by mounts,
     * but it can also be referenced by other things like open files.
     */
    atomic_uint64_t mountCount;
} superblock_t;

/**
 * @brief Superblock operations structure.
 * @struct superblock_ops_t
 */
typedef struct superblock_ops
{
    /**
     * Called when the VFS needs to create a new inode, if not specified `heap_alloc()` is used.
     * This is usefull as it lets filesystems allocate a structure larget than `inode_t` and use the additional
     * space for private data in addition to the `private` pointer in `inode_t`.
     */
    inode_t* (*allocInode)(superblock_t* superblock);
    /**
     * Called when the VFS wants to free an inode, if not specified `free()` is used.
     */
    void (*freeInode)(superblock_t* superblock, inode_t* inode);
    /**
     * Called when the filesystem is superblock is being freed to give the filesystem a chance to clean up any private
     * data.
     */
    void (*cleanup)(superblock_t* superblock);
    /**
     * Called when the the superblocks `mountCount` reaches zero, meaning it is not visible anywhere in any namespace.
     */
    void (*unmount)(superblock_t* superblock);
} superblock_ops_t;

/**
 * @brief Create a new superblock.
 *
 * This does not add the superblock to the superblock cache, the `vfs_mount()` function will do that using
 * `vfs_add_superblock()`.
 *
 * There is no `superblock_free()` instead use `UNREF()`.
 *
 * Note that the superblock's `root` dentry must be created and assigned after calling this function.
 *
 * @param fs The filesystem type of the superblock.
 * @param device The device the superblock is mounted on.
 * @param ops The superblock operations, can be NULL.
 * @param dentryOps The dentry operations for dentries in this superblock, can be NULL.
 * @return On success, the new superblock. On failure, returns `NULL` and `errno` is set.
 */
superblock_t* superblock_new(filesystem_t* fs, dev_t device, const superblock_ops_t* ops,
    const dentry_ops_t* dentryOps);

/**
 * @brief Increment the mount count of a superblock.
 *
 * @param superblock Pointer to the superblock.
 */
void superblock_inc_mount_count(superblock_t* superblock);

/**
 * @brief Decrement the mount count of a superblock.
 *
 * If the mount count reaches zero, the `unmount` operation is called if its not `NULL`.
 *
 * @param superblock Pointer to the superblock.
 */
void superblock_dec_mount_count(superblock_t* superblock);

/** @} */
