#pragma once

#include <kernel/fs/diremit.h>
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
typedef struct dentry dentry_t;

/**
 * @brief Virtual node.
 * @defgroup kernel_fs_vnode Vnode
 * @ingroup kernel_fs
 *
 * @todo Update the virtual node documentation for the new class and IRP stuff.
 *
 * @todo Add system for allocating vnodes of different sizes to replace the `data` pointer.
 *
 * @{
 */

/**
 * @brief Vnode class structure.
 * @struct vnode_class_t
 *
 * Defines the behavior and I/O handlers for a specific class of vnodes, for example a ext4 filesystem might have a
 * "ext4 regular" class and a "ext4 directory" vnode class.
 *
 * @note The `VNODE_HANDLERS()` and `VNODE_DIR_HANDLERS()` macros should always be used when defining the handlers of a
 * class in order to ensure that appropriate defaults are always set.
 */
typedef struct vnode_class
{
    const char* name;                   ///< The name of the class, used for debugging.
    file_type_t type;                   ///< The type of the vnode.
    cache_t* cache; ///< The cache to allocate vnodes from, if `NULL` a default cache will be used.
    irp_handler_t handlers[IRP_MJ_MAX]; ///< IRP handlers indexed by major function number.
    /**
     * @brief Called when the dentry is looked up or retrieved from cache.
     *
     * Used for security by hiding files or directories based on filesystem defined logic.
     *
     * @return `true` if the access should be allowed, `false` otherwise.
     */
    bool (*access)(dentry_t* dentry);
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
    void* data;           ///< Filesystem defined data.
    file_volume_t volume; ///< The id of the volume the vnode belongs to.
    file_number_t number; ///< The number of the vnode, should be unique within the volume.
    const vnode_class_t* cls;
    mutex_t mutex;
    irp_t* reclaim; ///< Pre-allocated IRP used to reclaim the vnode, needed to avoid out of memory errors when
                    /// reclaiming a vnode.
} vnode_t;

/**
 * @brief Macro to safely type cast a vnode to its filesystem-specific structure.
 *
 * @param _vnode The vnode to type cast.
 * @param _type The type of the filesystem-specific vnode.
 */
#define VNODE_GET(_vnode, _type) ({ \
    assert((_vnode) == NULL || sizeof(_type) >= (_vnode)->cls != NULL ? (_vnode)->cls->cache->size : sizeof(vnode_t)); \
    (_type*)(_vnode); \
})

/**
 * @brief Create a new vnode.
 *
 * Does not associate the vnode with a dentry, that is done when a dentry is made positive with
 * `dentry_make_positive()`.
 *
 * There is no `vnode_free()` instead use `UNREF()`.
 *
 * @param volume The id of the volume the vnode belongs to.
 * @param cls The vnode class defining its behaviour.
 * @param number The number of the vnode, should be unique within the volume.
 * @return On success, the new vnode. On failure, returns `NULL`.
 */
vnode_t* vnode_new(file_volume_t volume, const vnode_class_t* cls, file_number_t number);

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

/**
 * @brief Generic attribute handler.
 *
 * This function can be used as a default `IRP_MJ_ATTR` handler.
 *
 * @param irp The IRP.
 * @return An appropriate status value.
 */
status_t vnode_generic_attr(irp_t* irp);

/**
 * @brief Generic query handler.
 *
 * This function can be used as a default `IRP_MJ_QUERY` handler.
 *
 * @param irp The IRP.
 * @return An appropriate status value.
 */
status_t vnode_generic_query(irp_t* irp);

/**
 * @brief Generic directory read handler.
 *
 * This function can be used as a default `IRP_MJ_READ` handler for directory vnodes.
 *
 * @param irp The IRP.
 * @return An appropriate status value.
 */
status_t vnode_generic_dir_read(irp_t* irp);

/**
 * @brief Generic lookup handler.
 * 
 * * This function can be used as a default `IRP_MJ_LOOKUP` handler.
 *
 * @param irp The IRP.
 * @return Always `ERR(FS, NOENT)`.
 */
status_t vnode_generic_lookup(irp_t* irp);

/**
 * @brief Helper macro to define default vnode handlers.
 *
 * This macro should be used for all non-directory vnodes.
 */
#define VNODE_HANDLERS() [IRP_MJ_ATTR] = vnode_generic_attr, [IRP_MJ_QUERY] = vnode_generic_query

/**
 * @brief Helper macro to define default directory vnode handlers.
 *
 * This macro should be used for all directory vnodes.
 */
#define VNODE_DIR_HANDLERS() \
    [IRP_MJ_READ] = vnode_generic_dir_read, [IRP_MJ_ATTR] = vnode_generic_attr, [IRP_MJ_QUERY] = vnode_generic_query, [IRP_MJ_LOOKUP] = vnode_generic_lookup


/**
 * @brief Generate a unique vnode number based on the parent vnode and the name of the file.
 *
 * Intended as a helper for filesystems that do not have some form of number that uniquely identifies a file, for
 * example, devfs.
 *
 * @param parent The vnode number of the parent directory.
 * @param name The name of the file.
 * @return The generated vnode number.
 */
file_number_t vnode_hash(file_number_t parent, const char* name);

/** @} */
