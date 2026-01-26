#pragma once

#include <kernel/fs/path.h>
#include <kernel/mem/paging_types.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>
#include <stdint.h>
#include <sys/fs.h>
#include <sys/proc.h>
#include <sys/status.h>

typedef struct wait_queue wait_queue_t;

typedef struct file file_t;
typedef struct file_ops file_ops_t;
typedef struct dentry dentry_t;
typedef struct vnode vnode_t;
typedef struct poll_file poll_file_t;

/**
 * @brief Underlying type of a file descriptor.
 * @defgroup kernel_fs_file File
 * @ingroup kernel_fs
 *
 * A file is the underlying type of a file descriptor. Note that internally the kernel does not use file descriptors,
 * they are simply a per-process handle to a file. The kernel uses files directly.
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
    ref_t ref;
    size_t pos;
    mode_t mode;
    vnode_t* vnode;
    path_t path;
    const file_ops_t* ops;
    void* data;
} file_t;

/**
 * @brief File operations structure.
 * @struct file_ops_t
 */
typedef struct file_ops
{
    status_t (*open)(file_t* file);
    status_t (*open2)(file_t* files[2]);
    void (*close)(file_t* file);
    status_t (*read)(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead);
    status_t (*write)(file_t* file, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten);
    status_t (*seek)(file_t* file, ssize_t offset, seek_origin_t origin, size_t* newPos);
    status_t (*ioctl)(file_t* file, uint64_t request, void* argp, size_t size, uint64_t* result);
    status_t (*poll)(file_t* file, poll_events_t* revents, wait_queue_t** queue);
    status_t (*mmap)(file_t* file, void** address, size_t length, size_t* offset, pml_flags_t flags);
} file_ops_t;

/**
 * @brief Structure for polling multiple files.
 * @struct poll_file_t
 */
typedef struct poll_file
{
    file_t* file;
    poll_events_t events;
    poll_events_t revents;
} poll_file_t;

/**
 * @brief Create a new file structure.
 *
 * This does not open the file, instead its used internally by the VFS when opening files.
 *
 * There is no `file_free()` instead use `UNREF()`.
 *
 * @param path The path of the file.
 * @param mode The mode with which the file was opened, if no permissions are specified the maximum allowed permissions
 * from the mount are used.
 * @return On success, a pointer to the allocated file. On failure, `NULL`.
 */
file_t* file_new(const path_t* path, mode_t mode);

/**
 * @brief Helper function for basic seeking.
 *
 * This can be used by filesystems that do not have any special requirements for seeking.
 *
 * Used by setting the file ops seek to this function.
 */
status_t file_generic_seek(file_t* file, ssize_t offset, seek_origin_t origin, size_t* newPos);

/** @} */
