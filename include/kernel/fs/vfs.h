#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/sysfs.h>
#include <kernel/sched/process.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>

#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

/**
 * @brief Virtual File System.
 * @defgroup kernel_vfs Virtual File System
 * @ingroup kernel_fs
 *
 * @{
 */

/**
 * @brief The name of the root entry.
 */
#define VFS_ROOT_ENTRY_NAME "__root__"

/**
 * @brief The name used to indicate no device.
 */
#define VFS_DEVICE_NAME_NONE "__no_device__"

/**
 * @brief Open a file.
 *
 * @param pathname The pathname of the file to open.
 * @param process The process opening the file.
 * @return On success, the opened file. On failure, returns `NULL` and `errno` is set.
 */
file_t* vfs_open(const pathname_t* pathname, process_t* process);

/**
 * @brief Open one file, returning two file handles.
 *
 * Used primarily to implement pipes.
 *
 * @param pathname The pathname of the file to open.
 * @param files The output array of two file pointers.
 * @param process The process opening the file.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_open2(const pathname_t* pathname, file_t* files[2], process_t* process);

/**
 * @brief Open a file relative to another path.
 *
 * @param from The path to open the file relative to.
 * @param pathname The pathname of the file to open.
 * @param process The process opening the file.
 * @return On success, the opened file. On failure, returns `NULL` and `errno` is set.
 */
file_t* vfs_openat(const path_t* from, const pathname_t* pathname, process_t* process);

/**
 * @brief Read from a file.
 *
 * Follows POSIX semantics.
 *
 * @param file The file to read from.
 * @param buffer The buffer to read into.
 * @param count The number of bytes to read.
 * @return On success, the number of bytes read. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_read(file_t* file, void* buffer, uint64_t count);

/**
 * @brief Write to a file.
 *
 * Follows POSIX semantics.
 *
 * @param file The file to write to.
 * @param buffer The buffer to write from.
 * @param count The number of bytes to write.
 * @return On success, the number of bytes written. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_write(file_t* file, const void* buffer, uint64_t count);

/**
 * @brief Seek in a file.
 *
 * Follows POSIX semantics.
 *
 * @param file The file to seek in.
 * @param offset The offset to seek to.
 * @param origin The origin to seek from.
 * @return On success, the new file position. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_seek(file_t* file, int64_t offset, seek_origin_t origin);

/**
 * @brief Perform an ioctl operation on a file.
 *
 * @param file The file to perform the ioctl on.
 * @param request The ioctl request.
 * @param argp The argument pointer.
 * @param size The size of the argument.
 * @return On success, the result of the ioctl. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_ioctl(file_t* file, uint64_t request, void* argp, uint64_t size);

/**
 * @brief Memory map a file.
 *
 * @param file The file to memory map.
 * @param address The address to map to, or `NULL` to let the kernel choose.
 * @param length The length to map.
 * @param flags The page table flags for the mapping.
 * @return On success, the mapped address. On failure, returns `NULL` and `errno` is set.
 */
void* vfs_mmap(file_t* file, void* address, uint64_t length, pml_flags_t flags);

/**
 * @brief Poll multiple files.
 *
 * @param files The array of files to poll.
 * @param amount The number of files in the array.
 * @param timeout The timeout in clock ticks, or `CLOCKS_NEVER` to wait indefinitely.
 * @return On success, the number of files that are ready. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_poll(poll_file_t* files, uint64_t amount, clock_t timeout);

/**
 * @brief Get directory entries from a directory file.
 *
 * @param file The directory file to read from.
 * @param buffer The buffer to read into.
 * @param count The number of bytes to read.
 * @return On success, the number of bytes read. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_getdents(file_t* file, dirent_t* buffer, uint64_t count);

/**
 * @brief Get file information.
 *
 * @param pathname The pathname of the file to get information about.
 * @param buffer The buffer to store the file information in.
 * @param process The process performing the stat.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_stat(const pathname_t* pathname, stat_t* buffer, process_t* process);

/**
 * @brief Make the same file appear twice in the filesystem.
 *
 * @param oldPathname The existing file.
 * @param newPathname The new link to create, must not exist and be in the same filesystem as the oldPathname.
 * @param process The process performing the linking.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_link(const pathname_t* oldPathname, const pathname_t* newPathname, process_t* process);

/**
 * @brief Remove a file or directory.
 *
 * @param pathname The pathname of the file or directory to remove.
 * @param process The process performing the removal.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_remove(const pathname_t* pathname, process_t* process);

/**
 * @brief Generates a new unique ID, to be used for any VFS object.
 *
 * @return A new unique ID.
 */
uint64_t vfs_get_new_id(void);

/**
 * @brief Helper macros for implementing file operations dealing with simple buffers.
 *
 * @param buffer The destination buffer.
 * @param count The number of bytes to read/write.
 * @param offset A pointer to the current offset, will be updated.
 * @param src The source buffer.
 * @param size The size of the source buffer.
 * @return The number of bytes read/written.
 */
#define BUFFER_READ(buffer, count, offset, src, size) \
    ({ \
        uint64_t readCount = (*(offset) <= (size)) ? MIN((count), (size) - *(offset)) : 0; \
        memcpy((buffer), (src) + *(offset), readCount); \
        *(offset) += readCount; \
        readCount; \
    })

/**
 * @brief Helper macro for implementing file operations dealing with simple buffer writes.
 *
 * @param buffer The destination buffer.
 * @param count The number of bytes to write.
 * @param offset A pointer to the current offset, will be updated.
 * @param src The source buffer.
 * @param size The size of the source buffer.
 * @return The number of bytes written.
 */
#define BUFFER_WRITE(buffer, count, offset, src, size) \
    ({ \
        uint64_t writeCount = (*(offset) <= (size)) ? MIN((count), (size) - *(offset)) : 0; \
        memcpy((buffer) + *(offset), (src), writeCount); \
        *(offset) += writeCount; \
        writeCount; \
    })

/** @} */
