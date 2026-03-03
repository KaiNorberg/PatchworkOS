#pragma once

#include <kernel/fs/path.h>
#include <kernel/io/irp.h>
#include <kernel/mem/paging_types.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>
#include <stdint.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <sys/status.h>

typedef struct wait_queue wait_queue_t;

typedef struct file file_t;
typedef struct dentry dentry_t;
typedef struct vnode vnode_t;
typedef struct poll_file poll_file_t;

/**
 * @brief Underlying type of a file descriptor.
 * @defgroup kernel_fs_file File
 * @ingroup kernel_fs
 *
 * A file is the underlying type of a file descriptor, representing a specific location or path within the filesystem.
 *
 * @note Files are distinct from "regular files". A file is simply any object that can be interacted with using standard
 * file operations (read, write, etc.). A regular file is a specific type of file that exists on a filesystem.
 *
 * @{
 */

/**
 * @brief File structure.
 * @struct file_t
 *
 * A file structure is protected by the mutex of its vnode.
 *
 */
typedef struct file
{
    ref_t ref; ///< Reference counting.
    size_t pos; ///< The current file position.
    mode_t mode; ///< Specifies permissions and file behaviour.
    uint8_t _reserved[4];
    path_t path; ///< The opened path.
    void* data; ///< Private filesystem data.
    irp_t* close; ///< Pre-allocated IRP used to close the file, needed to avoid out of memory errors when closing a file.
} file_t;

/**
 * @brief Create a new file structure.
 *
 * This does not open the file, instead its used internally by the VFS when opening files.
 *
 * There is no `file_free()` instead use `UNREF()`.
 *
 * @param dentry The dentry of the file to open.
 * @param mount The mount of the file to open.
 * @param mode The mode with which the file was opened, if no permissions are specified the maximum allowed permissions
 * from the mount are used.
 * @return On success, a pointer to the allocated file. On failure, `NULL`.
 */
file_t* file_new(dentry_t* dentry, binding_t* mount, mode_t mode);

/**
 * @brief Send an IRP to the vnode of the specified file.
 *
 * Will advance the IRP stack.
 *
 * @param file The file to associated with the next IRP stack frame.
 * @param irp The IRP to send.
 * @return An appropriate status value.
 */
status_t file_call(file_t* file, irp_t* irp);

/** @} */
