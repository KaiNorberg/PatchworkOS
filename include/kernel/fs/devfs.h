#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/vnode.h>
#include <sys/fs.h>

typedef struct file file_t;

typedef struct volume volume_t;
typedef struct volume_ops volume_ops_t;

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
 * @brief Create a new dentry inside a mounted devfs instance.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new dentry.
 * @param cls The class to assign to the created vnode.
 * @param data Private data to store in the vnode of the new dentry, can be `NULL`.
 * @return On success, the new devfs dentry. On failure, `NULL`.
 */
dentry_t* devfs_dentry_new(dentry_t* parent, const char* name, const vnode_class_t* cls, void* data);

/**
 * @brief Descriptor for batch dentry creation.
 * @struct devfs_desc_t
 */
typedef struct devfs_desc
{
    const char* name;         ///< Name of the dentry.
    const vnode_class_t* cls; ///< Class to assign to the vnode of the created dentry.
    void* data;               ///< Private data to store in the vnode of the dentry.
} devfs_desc_t;

/**
 * @brief Create multiple dentrys in a devfs directory.
 *
 * @param out Output list to store created dentries, can be `NULL`. The dentries use the `otherEntry` list entry.
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param descs Array of devfs descriptors.
 * @param count The number of dentrys to create.
 * @return `true` on success, `false` on failure.
 */
bool devfs_dentrys_new(list_t* out, dentry_t* parent, const devfs_desc_t* descs, size_t count);

/**
 * @brief Free all dentrys in a list created by `devfs_dentrys_new()`.
 *
 * @param dentrys The list of dentrys to free.
 */
void devfs_dentrys_free(list_t* dentrys);

/** @} */
