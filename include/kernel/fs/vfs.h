#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/file.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/sysfs.h>
#include <kernel/proc/process.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>

#include <sys/io.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

typedef struct filesystem filesystem_t;

/**
 * @brief Virtual File System.
 * @defgroup kernel_fs Virtual File System
 * @ingroup kernel_fs
 *
 * TODO: Implement improved caching, LRU.
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
 * @brief Filesystem structure, represents a filesystem type, e.g. fat32, ramfs, sysfs, etc.
 */
typedef struct filesystem
{
    list_entry_t entry;
    const char* name;
    dentry_t* (*mount)(filesystem_t* fs, const char* devName, void* private);
} filesystem_t;

/**
 * @brief Helper structure for lists with a lock.
 */
typedef struct
{
    list_t list;
    rwlock_t lock;
} vfs_list_t;

/**
 * @brief Helper structure for maps with a lock.
 */
typedef struct
{
    map_t map;
    rwlock_t lock;
} vfs_map_t;

/**
 * @brief Initializes the VFS.
 */
void vfs_init(void);

/**
 * @brief Generates a new unique ID.
 * @return A new unique ID.
 */
uint64_t vfs_get_new_id(void);

/**
 * @brief Registers a filesystem.
 *
 * @param fs The filesystem to register.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_register_fs(filesystem_t* fs);

/**
 * @brief Unregisters a filesystem.
 *
 * @param fs The filesystem to unregister.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_unregister_fs(filesystem_t* fs);

/**
 * @brief Gets a filesystem by name.
 *
 * @param name The name of the filesystem.
 * @return On success, the filesystem. On failure, returns `NULL`, does not set `errno`.
 */
filesystem_t* vfs_get_fs(const char* name);

/**
 * @brief Get an inode for the given superblock and inode number.
 *
 * Note that there is a period of time where a inodes reference count has dropped to zero but its free function has not
 * had the time to remove it from the cache yet. In this case, this function will return `NULL` and set `errno` to
 * `ESTALE`.
 *
 * @param superblock The superblock.
 * @param number The inode number.
 * @return On success, the inode. On failure, returns `NULL` and `errno` is set.
 */
inode_t* vfs_get_inode(superblock_t* superblock, inode_number_t number);

/**
 * @brief Get a dentry for the given name. Will NOT traverse mountpoints.
 *
 * Note that there is a period of time where a dentrys reference count has dropped to zero but its free function has not
 * had the time to remove it from the cache yet. In this case, this function will return `NULL` and set `errno` to
 * `ESTALE`.
 *
 * @param parent The parent path.
 * @param name The name of the dentry.
 * @return On success, the dentry, might be negative. On failure, returns `NULL` and `errno` is set.
 */
dentry_t* vfs_get_dentry(const dentry_t* parent, const char* name);

/**
 * @brief Get or lookup a dentry for the given name. Will NOT traverse mountpoints.
 *
 * @param parent The parent path.
 * @param name The name of the dentry.
 * @return On success, the dentry, might be negative. On failure, returns `NULL` and `errno` is set.
 */
dentry_t* vfs_get_or_lookup_dentry(const path_t* parent, const char* name);

/**
 * @brief Add a inode to the inode cache.
 *
 * Should not be used manually, as it will be called in `inode_new()`.
 *
 * @param inode The inode to add.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_add_inode(inode_t* inode);

/**
 * @brief Add a dentry to the dentry cache.
 *
 * Should not be used manually, instead use `dentry_make_positive()`.
 *
 * @param dentry The dentry to add.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_add_dentry(dentry_t* dentry);

/**
 * @brief Remove a superblock from the superblock list.
 *
 * @param superblock The superblock to remove.
 */
void vfs_remove_superblock(superblock_t* superblock);

/**
 * @brief Remove an inode from the inode cache.
 *
 * @param inode The inode to remove.
 */
void vfs_remove_inode(inode_t* inode);

/**
 * @brief Remove a dentry from the dentry cache.
 *
 * @param dentry The dentry to remove.
 */
void vfs_remove_dentry(dentry_t* dentry);

/**
 * @brief Walk a pathname to a path, starting from the current process's working directory.
 *
 * @param outPath The output path.
 * @param pathname The pathname to walk.
 * @param flags Flags for the path walk.
 * @param process The process whose namespace and working directory to use.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_walk(path_t* outPath, const pathname_t* pathname, walk_flags_t flags, process_t* process);

/**
 * @brief Walk a pathname to its parent path, starting from the current process's working directory.
 *
 * @param outPath The output parent path.
 * @param pathname The pathname to walk.
 * @param outLastName The output last component name.
 * @param flags Flags for the path walk.
 * @param process The process whose namespace and working directory to use.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_walk_parent(path_t* outPath, const pathname_t* pathname, char* outLastName, walk_flags_t flags,
    process_t* process);

/**
 * @brief Walk a pathname to path and its parent path, starting from the current process's working directory.
 *
 * @param outParent The output parent path.
 * @param outChild The output path.
 * @param pathname The pathname to walk.
 * @param flags Flags for the path walk.
 * @param process The process whose namespace and working directory to use.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_walk_parent_and_child(path_t* outParent, path_t* outChild, const pathname_t* pathname, walk_flags_t flags,
    process_t* process);

/**
 * @brief Check if a name is valid.
 *
 * A valid name is not "." or "..", only contains chars considered valid by `PATH_VALID_CHAR`, and is not longer than
 * `MAX_NAME - 1`.
 *
 * @param name The name to check.
 * @return `true` if the name is valid, `false` otherwise.
 */
bool vfs_is_name_valid(const char* name);

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
 * Used to for example implement pipes.
 *
 * @param pathname The pathname of the file to open.
 * @param files The output array of two file pointers.
 * @param process The process opening the file.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t vfs_open2(const pathname_t* pathname, file_t* files[2], process_t* process);

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

/** @} */
