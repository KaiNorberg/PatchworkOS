#pragma once

#include "dentry.h"
#include "inode.h"

typedef struct file file_t;
typedef struct file_ops file_ops_t;

typedef struct sysfs_group sysfs_group_t;

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
 * @return The default SysFS directory.
 */
dentry_t* sysfs_get_default(void);

/**
 * @brief Mount a new instance of SysFS.
 *
 * Used to, for example, create `/dev`, `/proc` and directories whose contents should only be visible within a
 * specific namespace.
 *
 * Note that if the resulting `mount_t` is dereferenced, the SysFS instance will NOT be unmounted. To unmount the SysFS
 * instance, use `namespace_unmount()`. However, if the SysFS instance has already been unmounted elsewhere, for example
 * by its namespace being deinitialized, dereferencing the `mount_t` will unmount it.
 *
 * In practice this means that if you want the mount to exist for the lifetime of the process which owns the namespace,
 * you can just dereference the returned `mount_t` immediately. Otherwise it will exist until both the process has
 * exited and you have dereferenced the `mount_t`.
 *
 * @param parent The parent dentry to mount the SysFS under. If `NULL`, the SysFS will be mounted at the root of the
 * namespace.
 * @param name The name of the mount point.
 * @param ns The namespace to mount the SysFS in. If `NULL`, the kernel process's namespace is used.
 * @return On success, the mounted SysFS instance. On failure, `NULL` and `errno` is set.
 */
mount_t* sysfs_mount_new(dentry_t* parent, const char* name, namespace_t* ns);

/**
 * @brief Create a new directory inside a mounted SysFS instance.
 *
 * Used to, for example, create a new directory for a device or resource inside `/dev` or `/proc`.
 *
 * Note that if the resulting `dentry_t` is dereferenced, the directory will NOT be removed since its parent dentry will
 * hold a reference to it, meaning its parent must be dereferenced first. This is quite convenient as you can simply
 * keep track of the parent dentry and dereference it when you want to remove all its children.
 *
 * @param parent The parent directory.
 * @param name The name of the new directory.
 * @param inodeOps The inode operations for the new directory, can be `NULL`.
 * @param private Private data associated with the new directory, can be `NULL`.
 * @return On success, the new SysFS directory. On failure, `NULL` and `errno` is set.
 */
dentry_t* sysfs_directory_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, void* private);

/**
 * @brief Create a new file inside a mounted SysFS instance.
 *
 * Used to, for example, create a new file for a device or resource inside `/dev` or `/proc`.
 *
 * The behaviour of the resulting `dentry_t` reference is identical to `sysfs_directory_new()`.
 *
 * @param parent The parent directory.
 * @param name The name of the new file.
 * @param inodeOps The inode operations for the new file, can be `NULL`.
 * @param fileOps The file operations for the new file, can be `NULL`.
 * @param private Private data associated with the new file, can be `NULL`.
 * @return On success, the new SysFS file. On failure, `NULL` and `errno` is set.
 */
dentry_t* sysfs_file_new(dentry_t* parent, const char* name, const inode_ops_t* inodeOps, const file_ops_t* fileOps,
    void* private);

/** @} */
