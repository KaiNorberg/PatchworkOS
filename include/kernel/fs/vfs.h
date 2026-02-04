#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vnode.h>
#include <kernel/fs/volume.h>
#include <kernel/proc/process.h>
#include <kernel/sync/rwlock.h>

#include <sys/fs.h>
#include <sys/list.h>
#include <sys/map.h>
#include <sys/math.h>
#include <sys/proc.h>

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
 * @brief Open a file.
 *
 * @param out Output pointer for the opened file.
 * @param pathname The pathname of the file to open.
 * @param process The process opening the file.
 * @return An appropriate status value.
 */
status_t vfs_open(file_t** out, const pathname_t* pathname, process_t* process);

/**
 * @brief Open a file relative to another path.
 *
 * @param out Output pointer for the opened file.
 * @param from The path to open the file relative to, or `NULL` to use the process's current working directory.
 * @param pathname The pathname of the file to open.
 * @param process The process opening the file.
 * @return An appropriate status value.
 */
status_t vfs_openat(file_t** out, const path_t* from, const pathname_t* pathname, process_t* process);

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
status_t vfs_seek(file_t* file, ssize_t offset, iowhence_t origin, size_t* out);

/**
 * @brief Get directory entries from a directory file.
 *
 * @param file The directory file to read from.
 * @param buffer The buffer to read into.
 * @param count The number of bytes to read.
 * @param bytesRead Output pointer for the number of bytes read.
 * @return An appropriate status value.
 */
status_t vfs_getdents(file_t* file, dirent_t* buffer, size_t count, size_t* bytesRead);

/**
 * @brief Get file information.
 *
 * @param pathname The pathname of the file to get information about.
 * @param buffer The buffer to store the file information in.
 * @param process The process performing the stat.
 * @return An appropriate status value.
 */
status_t vfs_stat(const pathname_t* pathname, stat_t* buffer, process_t* process);

/**
 * @brief Make the same file appear twice in the filesystem.
 *
 * @param oldPathname The existing file.
 * @param newPathname The new link to create, must not exist and be in the same filesystem as the oldPathname.
 * @param process The process performing the linking.
 * @return An appropriate status value.
 */
status_t vfs_link(const pathname_t* oldPathname, const pathname_t* newPathname, process_t* process);

/**
 * @brief Read the path in a symbolic link.
 *
 * @param symlink The symbolic link vnode.
 * @param buffer The buffer to store the path in.
 * @param size The size of the buffer.
 * @param bytesRead Output pointer for the number of bytes read.
 * @return An appropriate status value.
 */
status_t vfs_readlink(vnode_t* symlink, char* buffer, size_t size, size_t* bytesRead);

/**
 * @brief Create a symbolic link.
 *
 * @param target The pathname to which the symbolic link will point.
 * @param linkpath The pathname of the symbolic link to create.
 * @param process The process performing the symlink creation.
 * @return An appropriate status value.
 */
status_t vfs_symlink(const pathname_t* target, const pathname_t* linkpath, process_t* process);

/**
 * @brief Remove a file or directory.
 *
 * @param pathname The pathname of the file or directory to remove.
 * @param process The process performing the removal.
 * @return An appropriate status value.
 */
status_t vfs_remove(const pathname_t* pathname, process_t* process);

/**
 * @brief Generates a new unique ID, to be used for any VFS object.
 *
 * @return A new unique ID.
 */
uint64_t vfs_id_get(void);

/** @} */
