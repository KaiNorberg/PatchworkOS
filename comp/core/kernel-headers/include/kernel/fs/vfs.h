#pragma once

#include <kernel/fs/binding.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vnode.h>
#include <kernel/proc/process.h>
#include <kernel/sync/rwlock.h>

#include <libc/fs.h>
#include <libc/list.h>
#include <libc/map.h>
#include <libc/math.h>
#include <libc/proc.h>

/**
 * @brief Virtual File System.
 * @defgroup kernel_fs Virtual File System
 * @ingroup kernel
 *
 * The Virtual File System (VFS) provides a single unified interface for any and all filesystems, including virtual
 * filesystems used to expose kernel resources to user space.
 *
 * @todo Most of this is going to be removed when the new IRP system is fully implemented, but for now its usefull to
 * keep it around during the refactor.
 *
 * @{
 */

/**
 * @brief Open a file relative to another path.
 *
 * @param out Output pointer for the opened file.
 * @param from The path to open the file relative to.
 * @param pathname The pathname of the file to open.
 * @param process The process opening the file.
 * @return An appropriate status value.
 */
status_t vfs_open(file_t** out, const path_t* from, const char* pathname, process_t* process);

/**
 * @brief Read from a file.
 *
 * Follows POSIX semantics.
 *
 * @param file The file to read from.
 * @param buffer The buffer to read into.
 * @param count The number of bytes to read.
 * @param out Output pointer for the number of bytes read.
 * @return An appropriate status value.
 */
status_t vfs_read(file_t* file, void* buffer, size_t count, size_t* out);

/**
 * @brief Write to a file.
 *
 * Follows POSIX semantics.
 *
 * @param file The file to write to.
 * @param buffer The buffer to write from.
 * @param count The number of bytes to write.
 * @param out Output pointer for the number of bytes written.
 * @return An appropriate status value.
 */
status_t vfs_write(file_t* file, const void* buffer, size_t count, size_t* out);

/**
 * @brief Seek in a file.
 *
 * Follows POSIX semantics.
 *
 * @param file The file to seek in.
 * @param offset The offset to seek to.
 * @param origin The origin to seek from.
 * @param out Output pointer for the new file position.
 * @return An appropriate status value.
 */
status_t vfs_seek(file_t* file, ssize_t offset, ioseek_t origin, size_t* out);

/**
 * @brief Generates a new unique ID, to be used for any VFS object.
 *
 * @return A new unique ID.
 */
uint64_t vfs_id_get(void);

/** @} */
