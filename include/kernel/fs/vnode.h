#pragma once

#include <kernel/fs/path.h>
#include <kernel/io/irp.h>
#include <kernel/sync/mutex.h>
#include <kernel/sync/rcu.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>
#include <stdint.h>
#include <sys/fs.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <time.h>

typedef struct vnode vnode_t;
typedef struct volume volume_t;
typedef struct dentry dentry_t;
typedef struct dir_ctx dir_ctx_t;

/**
 * @brief Virtual node.
 * @defgroup kernel_fs_vnode Vnode
 * @ingroup kernel_fs
 *
 * @todo Update the virtual node documentation for the new class and IRP stuff.
 *
 * A vnode represents the actual data and metadata of a file. It is referenced by dentries, which represent the name or
 * "location" of the file but a vnode can appear in multiple dentries due to hardlinks or mounts.
 *
 * ## Synchronization
 *
 * Vnodes have an additional purpose within the Virtual File System (VFS) as they act as the primary means of
 * synchronization. All dentries synchronize upon their vnodes mutex, open files synchronize upon the mutex of the
 * underlying vnode and operations like create, remove, etc synchronize upon the vnode mutex of the parent directory.
 *
 * @{
 */

/**
 * @brief Vnode class structure.
 * @struct vnode_class_t
 *
 * Defines the behavior and I/O handlers for a specific class of vnodes, for example a ext4 filesystem might have a
 * "ext4 regular" class and a "ext4 directory" vnode class.
 */
typedef struct vnode_class
{
    const char* name;                    ///< The name of the class, used for debugging.
    iotype_t type;                   ///< The type of the vnode.
    void (*close)(file_t* file);     ///< File destructor.
    irp_handler_t handlers[IRP_MJ_MAX];  ///< IRP handlers indexed by major function number.
    /**
     * @brief Cleanup function called when the vnode is being freed.
     *
     * @param vnode The vnode being freed.
     *
     * @deprecated Should be replaced as part of the async refactor.
     */
    void (*cleanup)(vnode_t* vnode);
    /**
     * @brief Called when the dentry is looked up or retrieved from cache.
     *
     * Used for security by hiding files or directories based on filesystem defined logic.
     *
     * @return `true` if the access should be allowed, `false` otherwise.
     *
     * @deprecated Should be replaced as part of the async refactor.
     */
    bool (*revalidate)(dentry_t* dentry);
} vnode_class_t;

/**
 * @brief vnode structure.
 * @struct vnode_t
 *
 * vnodes are owned by the filesystem, not the VFS.
 */
typedef struct vnode
{
    ref_t ref;
    _Atomic(uint64_t) dentryCount; ///< The number of dentries pointing to this vnode.
    void* data;                    ///< Filesystem defined data.
    uint64_t size;                 ///< Used for convenience by certain filesystems, does not represent the file size.
    volume_t* volume;
    const vnode_class_t* cls;
    rcu_entry_t rcu;
    mutex_t mutex;
} vnode_t;

/**
 * @brief Create a new vnode.
 *
 * Does not associate the vnode with a dentry, that is done when a dentry is made positive with
 * `dentry_make_positive()`.
 *
 * There is no `vnode_free()` instead use `UNREF()`.
 *
 * @param volume The volume the vnode belongs to.
 * @param cls The vnode class defining I/O its behaviour.
 * @return On success, the new vnode. On failure, returns `NULL`.
 */
vnode_t* vnode_new(volume_t* volume, const vnode_class_t* cls);

/**
 * @brief Send an IRP to a specified vnode.
 *
 * Will advance the IRP stack.
 *
 * @param vnode The vnode to associate with the next IRP stack frame.
 * @param irp The IRP to send.
 * @return An appropriate status value.
 */
status_t vnode_call(vnode_t* vnode, irp_t* irp);

/** @} */
