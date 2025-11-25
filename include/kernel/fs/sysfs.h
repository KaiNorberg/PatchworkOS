#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/inode.h>
#include <sys/io.h>

typedef struct file file_t;
typedef struct file_ops file_ops_t;

typedef struct superblock superblock_t;
typedef struct superblock_ops superblock_ops_t;

/**
 * @brief Filesystem for exposing kernel resources.
 * @ingroup kernel_fs
 * @defgroup kernel_fs_sysfs SysFS
 *
 * The SysFS filesystem is a convenient helper used by various subsystems to expose kernel resources to user space in
 * the filesystem. For example, the process subsystem uses SysFS to expose process information under `/proc`.
 *
 */

#define SYSFS_NAME "sysfs"

/**
 * @brief Initializes the SysFS.
 */
void sysfs_init(void);

/**
 * @brief Gets the default SysFS directory.
 *
 * The default SysFS directory is the root of the `/dev` mount. The `/dev` directory is for devices or "other"
 * resources which may not warrant an entire dedicated filesystem.
 *
 * @return A reference to the `/dev` SysFS directory.
 */
dentry_t* sysfs_get_dev(void);

/**
 * @brief Mount a new instance of SysFS.
 *
 * Used to, for example, create `/dev`, `/proc` and directories whose contents should only be visible within a
 * specific namespace.
 *
 * If `parent` is `NULL`, then the sysfs instance will be mounted to a already existing directory in the root of the
 * namespace. If it is not `NULL`, then it must point to a sysfs directory, a new directory of the name `name` will
 * be created inside it and the SysFS instance will be mounted there.
 *
 * @param parent The parent directory to mount the SysFS in. If `NULL`, the root of the namespace is used.
 * @param name The name of the directory to mount the SysFS in.
 * @param ns The namespace to mount the SysFS in, or `NULL` to use the current process's namespace.
 * @param flags Mount flags.
 * @param superblockOps The superblock operations for the new SysFS instance, can be `NULL`.
 * @return On success, the mounted SysFS instance. On failure, `NULL` and `errno` is set.
 */
mount_t* sysfs_mount_new(const path_t* parent, const char* name, namespace_t* ns, mount_flags_t flags,
    const superblock_ops_t* superblockOps);

/**
 * @brief Create a new directory inside a mounted SysFS instance.
 *
 * Used to, for example, create a new directory for a device or resource inside `/dev` or `/proc`.
 *
 * @param parent The parent directory, if `NULL` then `sysfs_get_dev()` is used.
 * @param name The name of the new directory.
 * @param inodeOps The inode operations for the new directory, can be `NULL`.
 * @param private Private data associated with the new directory, can be `NULL`, will be stored in the inode.
 * @return On success, the new SysFS directory. On failure, `NULL` and `errno` is set.
 */
dentry_t* sysfs_dir_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private);

/**
 * @brief Create a new file inside a mounted SysFS instance.
 *
 * Used to, for example, create a new file for a device or resource inside `/dev` or `/proc`.
 *
 * @param parent The parent directory, if `NULL` then `sysfs_get_dev()` is used.
 * @param name The name of the new file.
 * @param inodeOps The inode operations for the new file, can be `NULL`.
 * @param fileOps The file operations for the new file, can be `NULL`.
 * @param private Private data associated with the new file, can be `NULL`.
 * @return On success, the new SysFS file. On failure, `NULL` and `errno` is set.
 */
dentry_t* sysfs_file_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, const file_ops_t* fileOps,
    void* private);

/** @} */
