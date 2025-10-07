#pragma once

#include "dentry.h"
#include "inode.h"

typedef struct file file_t;
typedef struct file_ops file_ops_t;

/**
 * @brief Filesystem for exposing kernel resources.
 * @ingroup kernel_fs
 * @defgroup kernel_fs_sysfs SysFS
 *
 * The SysFS filesystem is a convenient helper used by various subsystems to expose kernel resources.
 *
 * A note about the distinction between dentrys and inodes. The dentrys in the SysFS are owned and managed by the SysFS,
 * these form the pseudo tree structure of the filesystem, meanwhile the inodes are to be considered as the actual
 * "object" being exposed by the SysFS and are owned by the subsystems. So if exposing a process then the file
 * `/proc/10/status` is a dentry, which stores the inode of the process status file.
 *
 */

#define SYSFS_NAME "sysfs"

/**
 * @brief Represents a file in a SysFS directory.
 */
typedef struct sysfs_file
{
    dentry_t* dentry;
} sysfs_file_t;

/**
 * @brief Represents a directory in a SysFS group.
 */
typedef struct sysfs_dir
{
    dentry_t* dentry;
} sysfs_dir_t;

/**
 * @brief Represents a mounted SysFS filesystem.
 *
 * A mounted SysFS filesystem is a virtual filesystem that exposes kernel resources. For example, `/dev`, and `/proc`
 * are two SysFS groups, the best way to think off it is that SysFS is the filesystem, and then each subsystem is the
 * "drive" that contains the files and directories. Instead of FAT32 on a SSD its SysFS on the process manager.
 *
 */
typedef struct sysfs_group
{
    sysfs_dir_t root;
    pathname_t mountpoint;
} sysfs_group_t;

/**
 * @brief Initializes the SysFS.
 */
void sysfs_init(void);

/**
 * @brief Gets the default SysFS directory.
 *
 * The default SysFS directory is the root of the `/dev` SysFS group. The `/dev` group is for devices or "other"
 * resources which may not warrant an entire group all by themselves.
 *
 * @return The default SysFS directory.
 */
sysfs_dir_t* sysfs_get_default(void);

/**
 * @brief Initializes a SysFS group.
 *
 * @param group The SysFS group to initialize.
 * @param mountpoint The mountpoint to attach the SysFS group to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t sysfs_group_init(sysfs_group_t* group, const pathname_t* mountpoint);

/**
 * @brief Deinitializes a SysFS group.
 *
 * @param group The SysFS group to deinitialize.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t sysfs_group_deinit(sysfs_group_t* group);

/**
 * @brief Initializes a SysFS directory.
 *
 * @param dir The SysFS directory to initialize.
 * @param parent The parent directory of the SysFS directory.
 * @param name The name of the SysFS directory.
 * @param inodeOps The inode operations for the SysFS directory, can be NULL.
 * @param private The private data for the SysFS directory, can be found in the inode after initialization, can be NULL.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t sysfs_dir_init(sysfs_dir_t* dir, sysfs_dir_t* parent, const char* name, const inode_ops_t* inodeOps,
    void* private);

/**
 * @brief Deinitializes a SysFS directory.
 *
 * @param dir The SysFS directory to deinitialize.
 */
void sysfs_dir_deinit(sysfs_dir_t* dir);

/**
 * @brief Initializes a SysFS file.
 *
 * @param file The SysFS file to initialize.
 * @param parent The parent directory of the SysFS file.
 * @param name The name of the SysFS file.
 * @param inodeOps The inode operations for the SysFS file, can be NULL.
 * @param fileOps The file operations for the SysFS file, can be NULL.
 * @param private The private data for the SysFS file, can be found in the inode after initialization, can be NULL.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t sysfs_file_init(sysfs_file_t* file, sysfs_dir_t* parent, const char* name, const inode_ops_t* inodeOps,
    const file_ops_t* fileOps, void* private);

/**
 * @brief Deinitializes a SysFS file.
 *
 * @param file The SysFS file to deinitialize.
 */
void sysfs_file_deinit(sysfs_file_t* file);

/** @} */
