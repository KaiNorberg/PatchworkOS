#pragma once

#include <kernel/fs/devfs.h>
#include <kernel/io/irp.h>
#include <kernel/utils/ref.h>

#include <stdint.h>
#include <sys/fs.h>
#include <sys/list.h>

typedef struct filesystem filesystem_t;
typedef struct volume volume_t;
typedef struct volume_ops volume_ops_t;
typedef struct vnode vnode_t;
typedef struct dentry dentry_t;

/**
 * @brief Filesystem Volume.
 * @defgroup kernel_fs_volume Volume
 * @ingroup kernel_fs
 *
 * A volume represents a single instance of a filesystem.
 *
 * @{
 */

/**
 * @brief Volume structure.
 * @struct volume_t
 */
typedef struct volume
{
    ref_t ref;
    list_entry_t entry;
    uint64_t id;
    void* data;
    dentry_t* root; ///< Root dentry of the filesystem, should not take a reference.
    const volume_ops_t* ops;
    filesystem_t* fs;
} volume_t;

/**
 * @brief Volume operations structure.
 * @struct volume_ops_t
 */
typedef struct volume_ops
{
    /**
     * Called when the filesystem is volume is being freed to give the filesystem a chance to clean up any private
     * data.
     */
    void (*cleanup)(volume_t* volume);
} volume_ops_t;

/**
 * @brief Create a new volume.
 *
 * This does not add the volume to the volume cache, the `vfs_mount()` function will do that using
 * `vfs_add_volume()`.
 *
 * There is no `volume_free()` instead use `UNREF()`.
 *
 * Note that the volume's `root` dentry must be created and assigned after calling this function.
 *
 * @param fs The filesystem type of the volume.
 * @param ops The volume operations, can be NULL.
 * @return On success, the new volume. On failure, returns `NULL`.
 */
volume_t* volume_new(filesystem_t* fs, const volume_ops_t* ops);

/** @} */
