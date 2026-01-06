#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/inode.h>
#include <sys/io.h>

typedef struct file file_t;
typedef struct file_ops file_ops_t;

typedef struct superblock superblock_t;
typedef struct superblock_ops superblock_ops_t;

/**
 * @brief Device Filesystem.
 * @ingroup kernel_fs
 * @defgroup kernel_fs_devfs Device Filesystem
 *
 */

/**
 * @brief The name of the device filesystem.
 */
#define DEVFS_NAME "devfs"

/**
 * @brief Initializes the devfs.
 */
void devfs_init(void);

/**
 * @brief Create a new directory inside a mounted devfs instance.
 *
 * Used to, for example, create a new directory for a device or resource inside `/dev` or `/proc`.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new directory.
 * @param inodeOps The inode operations for the new directory, can be `NULL`.
 * @param private Private data to store in the inode of the new directory, can be `NULL`.
 * @return On success, the new devfs directory. On failure, `NULL` and `errno` is set.
 */
dentry_t* devfs_dir_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private);

/**
 * @brief Create a new file inside a mounted devfs instance.
 *
 * Used to, for example, create a new file for a device or resource inside `/dev` or `/proc`.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new file.
 * @param inodeOps The inode operations for the new file, can be `NULL`.
 * @param fileOps The file operations for the new file, can be `NULL`.
 * @param private Private data to store in the inode of the new file, can be `NULL`.
 * @return On success, the new devfs file. On failure, `NULL` and `errno` is set.
 */
dentry_t* devfs_file_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, const file_ops_t* fileOps,
    void* private);

/**
 * @brief Create a new symbolic link inside a mounted devfs instance.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new symbolic link.
 * @param inodeOps The inode operations for the new symbolic link.
 * @param private Private data to store in the inode of the new symbolic link, can be `NULL`.
 * @return On success, the new devfs symbolic link. On failure, `NULL` and `errno` is set.
 */
dentry_t* devfs_symlink_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private);

/** @} */
