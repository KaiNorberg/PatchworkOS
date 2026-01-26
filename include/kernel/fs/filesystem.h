#pragma once

#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/mount.h>
#include <kernel/fs/path.h>
#include <kernel/fs/superblock.h>
#include <kernel/fs/vnode.h>
#include <kernel/proc/process.h>
#include <kernel/sync/rwlock.h>

#include <sys/fs.h>
#include <sys/map.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

/**
 * @brief Filesystem interface.
 * @defgroup kernel_fs_filesystem Filesystem.
 * @ingroup kernel_fs
 *
 * The filesystem interface represents a filesystem type, e.g. fat32, tmpfs, devfs, etc. Each filesystem is exposed in a
 * directory within the `fs` sysfs directory named after the filesystem.
 *
 * The directory itself can be used to mount instances of that filesystem type.
 *
 * Within each filesystem directory are readable files representing each mounted instance of that filesystem type, named
 * after the superblock ID, containing the following information:
 *
 * ```
 * id: %llu
 * block_size: %llu
 * max_file_size: %llu
 *
 * ```
 *
 * Where the `id` is the superblock ID, `block_size` is the block size of the superblock, and `max_file_size` is the
 * maximum size of a file on this superblock.
 *
 * @see kernel_fs_sysfs
 *
 * @{
 */

/**
 * @brief Filesystem structure, represents a filesystem type, e.g. fat32, tmpfs, devfs, etc.
 *
 * @todo Add safety for if a module defining a filesystem is unloaded.
 */
typedef struct filesystem
{
    list_entry_t entry;   ///< Used internally.
    map_entry_t mapEntry; ///< Used internally.
    list_t superblocks;   ///< Used internally.
    rwlock_t lock;        ///< Used internally.
    const char* name;
    /**
     * @brief Mount a filesystem.
     *
     * @param fs The filesystem to mount.
     * @param out pointer to store the root dentry of the mounted filesystem.
     * @param details A string containing filesystem defined `key=value` pairs, with multiple options separated by
     * commas, or `NULL`.
     * @param private Private data for the filesystem's mount function.
     * @return An appropriate status value.
     */
    status_t (*mount)(filesystem_t* fs, dentry_t** out, const char* details, void* data);
} filesystem_t;

/**s
 * @brief Exposes the sysfs `fs` directory.
 *
 * Must be called before `filesystem_get_by_path()` can be used.
 */
void filesystem_expose(void);

/**
 * @brief Registers a filesystem.
 *
 * @param fs The filesystem to register.
 * @return An appropriate status code.
 */
status_t filesystem_register(filesystem_t* fs);

/**
 * @brief Unregisters a filesystem.
 *
 * @param fs The filesystem to unregister, or `NULL` for no-op.
 */
void filesystem_unregister(filesystem_t* fs);

/**
 * @brief Gets a filesystem by name.
 *
 * @param name The name of the filesystem.
 * @return On success, the filesystem. On failure, returns `NULL`.
 */
filesystem_t* filesystem_get_by_name(const char* name);

/**
 * @brief Gets a filesystem by path.
 *
 * The path should point to a directory in the `fs` sysfs directory.
 *
 * @param path The path to check.
 * @param process The process whose namespace to use.
 * @return On success, the filesystem. On failure, returns `NULL`.
 */
filesystem_t* filesystem_get_by_path(const char* path, process_t* process);

/**
 * @brief Helper function for iterating over options passed to a filesystem mount operation.
 *
 * Each helper option is specified as `key=value` pairs, with multiple options separated by commas.
 *
 * @param iter Pointer to the current iterator position. Updated on each call.
 * @param buffer Buffer to store the current option.
 * @param size Size of the buffer.
 * @param key Pointer to store the key of the current option.
 * @param value Pointer to store the value of the current option.
 * @return `true` if an option was found, `false` if no more options are available.
 */
bool options_next(const char** iter, char* buffer, size_t size, char** key, char** value);

/**
 * @brief Helper macro for iterating over options passed to a filesystem mount operation.
 *
 * Each helper option is specified as `key=value` pairs, with multiple options separated by commas.
 *
 * @param options The options string.
 * @param key The key variable.
 * @param value The value variable.
 */
#define OPTIONS_FOR_EACH(options, key, value) \
    for (struct { \
             const char* iter; \
             char buf[256]; \
         } _state = {(options), {0}}; \
        options_next(&_state.iter, _state.buf, sizeof(_state.buf), &(key), &(value));)

/** @} */