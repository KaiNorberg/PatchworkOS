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
 * The devfs is a virtual filesystem that provides access to devices and resources.
 *
 * @{
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
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new directory.
 * @param inodeOps The inode operations for the new directory, can be `NULL`.
 * @param private Private data to store in the inode of the new directory, can be `NULL`.
 * @return On success, the new devfs directory. On failure, `NULL` and `errno` is set.
 */
dentry_t* devfs_dir_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps,  void* data);

/**
 * @brief Create a new file inside a mounted devfs instance.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new file.
 * @param inodeOps The inode operations for the new file, can be `NULL`.
 * @param fileOps The file operations for the new file, can be `NULL`.
 * @param private Private data to store in the inode of the new file, can be `NULL`.
 * @return On success, the new devfs file. On failure, `NULL` and `errno` is set.
 */
dentry_t* devfs_file_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, const file_ops_t* fileOps,
     void* data);

/**
 * @brief Create a new symbolic link inside a mounted devfs instance.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new symbolic link.
 * @param inodeOps The inode operations for the new symbolic link.
 * @param private Private data to store in the inode of the new symbolic link, can be `NULL`.
 * @return On success, the new devfs symbolic link. On failure, `NULL` and `errno` is set.
 */
dentry_t* devfs_symlink_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps,  void* data);

/**
 * @brief Descriptor for batch file creation.
 * @struct devfs_file_desc_t
 */
typedef struct devfs_file_desc
{
    const char* name;            ///< Name of the file, `NULL` marks end of array.
    const inode_ops_t* inodeOps; ///< Inode operations, can be `NULL`.
    const file_ops_t* fileOps;   ///< File operations, can be `NULL`.
    void* data;               ///< Private data to store in the inode of the file.
} devfs_file_desc_t;

/**
 * @brief Create multiple files in a devfs directory.
 *
 * @param out Output list to store created dentries, can be `NULL`. The dentries use the `otherEntry` list entry.
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param descs Array of file descriptors, terminated by an entry with `name == NULL`.
 * @return On success, the number of files created. On failure, `ERR` and `errno` is set.
 */
uint64_t devfs_files_new(list_t* out, dentry_t* parent, const devfs_file_desc_t* descs);

/**
 * @brief Free all files in a list created by `devfs_files_new()`.
 *
 * @param files The list of files to free.
 */
void devfs_files_free(list_t* files);

/** @} */
