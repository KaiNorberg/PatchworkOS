#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/vnode.h>
#include <sys/fs.h>

typedef struct file file_t;

typedef struct volume volume_t;
typedef struct volume_ops volume_ops_t;

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
 * @brief The volume ID reserved for sysfs.
 */
#define SYSFS_VOL 0

/**
 * @brief Initializes  the system filesystem.
 */
void sysfs_init(void);

/**
 * @brief Create a new dentry inside a mounted sysfs instance.
 *
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param name The name of the new dentry.
 * @param cls The class to assign to the created vnode.
 * @param data Private data to store in the vnode of the new dentry, can be `NULL`.
 * @return On success, the new sysfs dentry. On failure, `NULL`.
 */
dentry_t* sysfs_dentry_new(dentry_t* parent, const char* name, const vnode_class_t* cls, void* data);

/**
 * @brief Descriptor for batch dentry creation.
 * @struct sysfs_desc_t
 */
typedef struct sysfs_desc
{
    const char* name;    ///< Name of the dentry.
    const vnode_class_t* cls; ///< Class to assign to the vnode of the created dentry.
    void* data;          ///< Private data to store in the vnode of the dentry.
} sysfs_desc_t;

/**
 * @brief Create multiple dentries in a sysfs directory.
 *
 * @param out Output list to store created dentries, can be `NULL`. The dentries use the `entry` list entry.
 * @param parent The parent directory, if `NULL` then the root is used.
 * @param descs Array of sysfs descriptors.
 * @param count The number of dentries to create.
 * @return `true` on success, `false` on failure.
 */
bool sysfs_dentrys_new(list_t* out, dentry_t* parent, const sysfs_desc_t* descs, size_t count);

/**
 * @brief Free all dentries in a list created by `sysfs_dentrys_new()`.
 *
 * @param dentries The list of dentries to free.
 */
void sysfs_dentrys_free(list_t* dentries);

/** @} */
