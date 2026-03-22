#pragma once

#include <kernel/fs/binding.h>
#include <kernel/fs/dentry.h>
#include <kernel/fs/devfs.h>
#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vnode.h>
#include <kernel/proc/process.h>
#include <kernel/sync/rwlock.h>

#include <sys/fs.h>
#include <sys/list.h>
#include <sys/map.h>
#include <sys/math.h>
#include <sys/proc.h>

/**
 * @brief Filesystem structure.
 * @defgroup kernel_fs_filesystem Filesystem
 * @ingroup kernel_fs
 *
 * A filesystem defines the format and behaviour of volumes. For example, a fat32 filesystem would define how to read
 * and write files on a fat32 formatted volume, while a tmpfs filesystem would define how to manage an in-memory volume.
 *
 * @note There is not explicit "volume" structure, instead an opened filesystem simply returns the root of a heirarchy
 * representing the volume.
 *
 * ## Interacting with Filesystems
 *
 * All filesystems will have a directory within the `fs` directory of a `sysfs` instance. Included below is a list of
 * all files located within this directory.
 *
 * ## clone
 *
 * A special file that when opened creates a new opened instance of the filesystem, usually called a volume, with the
 * opened file being of type `FILE_TYPE_DIRECTORY` and storing the root of this volume.
 *
 * After opening this file, the returned file can be bound to complete a traditional mount operation or used directly.
 *
 * The payload specified in the path while opening this file is used to specify options for the new volume, for example
 * `/sys/fs/myfs/clone?option1=value1&option2=value2`. The interpretation of these options is up to the filesystem.
 *
 * @note For filesystems that do not support multiple volumes, the clone file may simply return the same root dentry
 * each time.
 *
 * @{
 */

/**
 * @brief Filesystem structure, represents a filesystem type, e.g. fat32, tmpfs, devfs, etc.
 * @struct filesystem_t
 *
 * The provided class should implement a `IRP_MJ_OPEN` handler that creates a new volume.
 */
typedef struct filesystem
{
    const char* name;           ///< The name of the filesystem.
    const vnode_class_t* clone; ///< The class to use for the filesystems clone file.
    struct
    {
        dentry_t* dir;   ///< The directory containing this filesystem.
        dentry_t* clone; ///< The clone file within this filesystems directory.
    } internal;
} filesystem_t;

/**
 * @brief Register a new filesystem.
 *
 * @param fs The filesystem structure to register.
 * @return An appropriate status value.
 */
status_t filesystem_register(filesystem_t* fs);

/**
 * @brief Unregister a filesystem.
 *
 * @param fs The filesystem structure to unregister.
 * @return An appropriate status value.
 */
status_t filesystem_unregister(filesystem_t* fs);

/**
 * @brief Generate a new unique volume ID.
 *
 * @return A new unique volume ID.
 */
file_volume_t volume_new(void);

/**
 * @brief Helper function for iterating over options passed to a filesystem mount operation.
 *
 * Each helper option is specified as `key=value` pairs, with multiple options separated by `&` characters.
 *
 * @param iter Pointer to the current iterator position. Updated on each call.
 * @param buffer Buffer to store the current option.
 * @param size Size of the buffer.
 * @param key Pointer to store the key of the current option.
 * @param value Pointer to store the value of the current option.
 * @return `true` if an option was found, `false` if no more options are available.
 */
bool options_next(const char** iter, char* buffer, size_t size, const char** key, char** value);

/**
 * @brief Helper macro for iterating over options passed to a filesystem mount operation.
 *
 * Each helper option is specified as `key=value` pairs, with multiple options separated by `&` characters.
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
