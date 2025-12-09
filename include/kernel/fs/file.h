#pragma once

#include <kernel/fs/path.h>
#include <kernel/mem/paging_types.h>
#include <kernel/utils/ref.h>

#include <stdatomic.h>
#include <stdint.h>
#include <sys/io.h>
#include <sys/proc.h>

typedef struct wait_queue wait_queue_t;

typedef struct file file_t;
typedef struct file_ops file_ops_t;
typedef struct dentry dentry_t;
typedef struct inode inode_t;
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
 * A file structure is protected by the mutex of its inode.
 *
 */
typedef struct file
{
    ref_t ref;
    uint64_t pos;
    mode_t mode;
    inode_t* inode;
    path_t path;
    const file_ops_t* ops;
    void* private;
} file_t;

/**
 * @brief File operations structure.
 * @struct file_ops_t
 */
typedef struct file_ops
{
    uint64_t (*open)(file_t* file);
    uint64_t (*open2)(file_t* files[2]);
    void (*close)(file_t* file);
    uint64_t (*read)(file_t* file, void* buffer, uint64_t count, uint64_t* offset);
    uint64_t (*write)(file_t* file, const void* buffer, uint64_t count, uint64_t* offset);
    uint64_t (*seek)(file_t* file, int64_t offset, seek_origin_t origin);
    uint64_t (*ioctl)(file_t* file, uint64_t request, void* argp, uint64_t size);
    wait_queue_t* (*poll)(file_t* file, poll_events_t* revents);
    void* (*mmap)(file_t* file, void* address, uint64_t length, uint64_t* offset, pml_flags_t flags);
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
 * There is no `file_free()` instead use `DEREF()`.
 *
 * @param path The path of the file.
 * @param mode The mode with which the file was opened, if no permissions are specified the maximum allowed permissions
 * from the mount are used.
 * @return On success, the new file. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EACCES`: The requested mode exceeds the maximum allowed permissions.
 * - `ENOENT`: The path is negative.
 * - `ENOMEM`: Out of memory.
 */
file_t* file_new(const path_t* path, mode_t mode);

/**
 * @brief Helper function for basic seeking.
 *
 * This can be used by filesystems that do not have any special requirements for seeking.
 *
 * Used by setting the file ops seek to this function.
 */
uint64_t file_generic_seek(file_t* file, int64_t offset, seek_origin_t origin);

/** @} */
