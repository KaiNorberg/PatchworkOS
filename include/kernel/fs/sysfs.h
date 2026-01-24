#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/vnode.h>
#include <sys/fs.h>

typedef struct file file_t;
typedef struct file_ops file_ops_t;

typedef struct superblock superblock_t;
typedef struct superblock_ops superblock_ops_t;

/**
 * @brief System Filesystem.
 * @ingroup kernel_fs
 * @defgroup kernel_fs_sysfs System Filesystem
 *
 * The sysfs is a virtual filesystem that provides information about devices and kernel modules.
 *
 * @{
 */

/**
 * @brief The name of the system filesystem.
 */
#define SYSFS_NAME "sysfs"

/**
 * @brief Initializes the sysfs and mount an instance at `/sys`.
 *
 * The System Filesystem is one of the few filesystem that will be mounted automatically by the kernel, this is
 * necessary as otherwise user space would be unable to access the `fs` sysfs directory and thus unable to mount any
 * filesystem.
 *
 */
void sysfs_init(void);

/**
 * @brief Create a new directory inside a mounted sysfs instance.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new directory.
 * @param vnodeOps The vnode operations for the new directory, can be `NULL`.
 * @param private Private data to store in the vnode of the new directory, can be `NULL`.
 * @return On success, the new sysfs directory. On failure, `NULL` and `errno` is set.
 */
dentry_t* sysfs_dir_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, void* data);

/**
 * @brief Create a new file inside a mounted sysfs instance.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new file.
 * @param vnodeOps The vnode operations for the new file, can be `NULL`.
 * @param fileOps The file operations for the new file, can be `NULL`.
 * @param private Private data to store in the vnode of the new file, can be `NULL`.
 * @return On success, the new sysfs file. On failure, `NULL` and `errno` is set.
 */
dentry_t* sysfs_file_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, const file_ops_t* fileOps,
    void* data);

/**
 * @brief Create a new symbolic link inside a mounted sysfs instance.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new symbolic link.
 * @param vnodeOps The vnode operations for the new symbolic link.
 * @param private Private data to store in the vnode of the new symbolic link, can be `NULL`.
 * @return On success, the new sysfs symbolic link. On failure, `NULL` and `errno` is set.
 */
dentry_t* sysfs_symlink_new(dentry_t* parent, const char* name, const vnode_ops_t* vnodeOps, void* data);

/**
 * @brief Descriptor for batch file creation.
 * @struct sysfs_file_desc_t
 */
typedef struct sysfs_file_desc
{
    const char* name;            ///< Name of the file, `NULL` marks end of array.
    const vnode_ops_t* vnodeOps; ///< Vnode operations, can be `NULL`.
    const file_ops_t* fileOps;   ///< File operations, can be `NULL`.
    void* data;                  ///< Private data to store in the vnode of the file.
} sysfs_file_desc_t;

/**
 * @brief Create multiple files in a sysfs directory.
 *
 * @param out Output list to store created dentries, can be `NULL`. The dentries use the `otherEntry` list entry.
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param descs Array of file descriptors, terminated by an entry with `name == NULL`.
 * @return On success, the number of files created. On failure, `ERR` and `errno` is set.
 */
uint64_t sysfs_files_new(list_t* out, dentry_t* parent, const sysfs_file_desc_t* descs);

/**
 * @brief Free all files in a list created by `sysfs_files_new()`.
 *
 * @param files The list of files to free.
 */
void sysfs_files_free(list_t* files);

/** @} */
