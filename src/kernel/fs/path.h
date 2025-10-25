#pragma once

#include "utils/map.h"

#include <alloca.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/io.h>

typedef struct path path_t;
typedef struct mount mount_t;
typedef struct dentry dentry_t;
typedef struct namespace namespace_t;

/**
 * @brief Unique location in the filesystem.
 * @defgroup kernel_fs_path Path
 * @ingroup kernel_fs
 *
 * A path is a single unique location in the filesystem hierarchy. It consists of a mount and a dentry. The mount is the
 * filesystem that the path is in and the dentry is the actual location in that filesystem.
 *
 * Note how just a dentry is not enough to uniquely identify a location in the filesystem, this is because of
 * mountpoints. A dentry can exist in multiple places if its part of a filesystem that has been mounted in multiple
 * places.
 *
 * TODO: Make path reference counting less error prone.
 *
 * @{
 */

/**
 * @brief Defer path put.
 *
 * This macro will call `path_put()` on the given path when it goes out of scope.
 *
 * @param path The path to defer.
 */
#define PATH_DEFER(path) __attribute__((cleanup(path_defer_cleanup))) path_t* CONCAT(i, __COUNTER__) = (path)

/**
 * @brief Check if a char is valid.
 *
 * A valid char is one of the following `abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.
 * ()[]{}~!@#$%^&?',;=+`.
 *
 * TODO: Replace with array lookup.
 *
 * @param ch The char to check.
 * @return true if the char is valid, false otherwise.
 */
#define PATH_VALID_CHAR(ch) \
    (strchr("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-. ()[]{}~!@#$%^&?',;=+", (ch)))

/**
 * @brief Maximum iterations to handle `..` in a path.
 *
 * This is to prevent infinite loops in case of a corrupted filesystem.
 */
#define PATH_HANDLE_DOTDOT_MAX_ITER 1000

/**
 * @brief Path flags.
 * @enum path_flags_t
 */
typedef enum
{
    PATH_NONE = 0,           ///< No flags.
    PATH_NONBLOCK = 1 << 0,  ///< Do not block on operations that would normally block.
    PATH_APPEND = 1 << 1,    ///< All writes will append to the end of the file.
    PATH_CREATE = 1 << 2,    ///< Create the file if it does not exist.
    PATH_EXCLUSIVE = 1 << 3, ///< When used with `PATH_CREATE`, fail if the file already exists.
    PATH_TRUNCATE = 1 << 4,  ///< Truncate the file to 0 length if it already exists.
    PATH_DIRECTORY = 1 << 5, ///< Fail if the path is not a directory, if not set then fail if it is a directory.
    PATH_RECURSIVE = 1 << 6, ///< Create parent directories if they do not exist when creating a file.
    PATH_FLAGS_AMOUNT = 7    ///< The amount of path flags.
} path_flags_t;

/**
 * @brief Flags for walking a path.
 * @enum walk_flags_t
 */
typedef enum
{
    WALK_NONE = 0,                    ///< No flags.
    WALK_NEGATIVE_IS_OK = 1 << 0,     ///< If a negative dentry is ok, if not specified then it is considered an error.
    WALK_MOUNTPOINT_TO_ROOT = 1 << 2, ///< If the pathname points to a mountpoint, return the root of the filesystem.
} walk_flags_t;

/**
 * @brief Path structure.
 * @struct path_t
 */
typedef struct path
{
    mount_t* mount;
    dentry_t* dentry;
} path_t;

/**
 * @brief Pathname structure.
 * @struct pathname_t
 *
 * A pathname is a string representation of a path.
 */
typedef struct pathname
{
    char string[MAX_PATH];
    path_flags_t flags;
    bool isValid;
} pathname_t;

/**
 * @brief Initialize path flags resolution.
 */
void path_flags_init(void);

/**
 * @brief Check if a pathname is valid.
 *
 * A valid pathname is not `NULL` and has its `isValid` flag set to true.
 *
 * This flag is set in `pathname_init()`.
 *
 * @param pathname The pathname to check.
 * @return true if the pathname is valid, false otherwise.
 */
#define PATHNAME_IS_VALID(pathname) ((pathname) != NULL && (pathname)->isValid)

/**
 * @brief Helper to create a pathname.
 *
 * This macro will create a pathname on the stack and initialize it with the given string.
 *
 * This is also the reason we have the `isValid` flag in the `pathname_t` structure, to be able to check if this macro
 * failed without having to return an error code, streamlining the code a bit.
 *
 * @param string The string to initialize the pathname with.
 * @return The initialized pathname.
 */
#define PATHNAME(string) \
    ({ \
        pathname_t* pathname = alloca(sizeof(pathname_t)); \
        pathname_init(pathname, string); \
        pathname; \
    })

/**
 * @brief Initialize a pathname.
 *
 * If the string is invalid, it will error and set pathname->isValid to false.
 *
 * @param pathname The pathname to initialize.
 * @param string The string to initialize the pathname with.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t pathname_init(pathname_t* pathname, const char* string);

/**
 * @brief Helper to create an empty path.
 *
 * Its important to always use this as some functions, for example `path_copy()`, will deref the existing mount and
 * dentry in the path.
 *
 * @return An empty path.
 */
#define PATH_EMPTY \
    (path_t) \
    { \
        .mount = NULL, .dentry = NULL \
    }

/**
 * @brief Helper to create a path.
 *
 * @param inMount The mount of the path.
 * @param inDentry The dentry of the path.
 * @return The created path.
 */
#define PATH_CREATE(inMount, inDentry) \
    (path_t) \
    { \
        .mount = REF(inMount), .dentry = REF(inDentry), \
    }

/**
 * @brief Set a path.
 *
 * Will deref the existing mount and dentry in the path if they are not `NULL`.
 *
 * @param path The path to set.
 * @param mount The mount to set.
 * @param dentry The dentry to set.
 */
void path_set(path_t* path, mount_t* mount, dentry_t* dentry);

/**
 * @brief Copy a path.
 *
 * Will deref the existing mount and dentry in the destination path if they are not `NULL`.
 *
 * @param dest The destination path.
 * @param src The source path.
 */
void path_copy(path_t* dest, const path_t* src);

/**
 * @brief Put a path.
 *
 * Will deref the mount and dentry in the path if they are not `NULL`.
 *
 * @param path The path to put.
 */
void path_put(path_t* path);

/**
 * @brief Traverse a single component from a parent path.
 *
 * @param outPath The output path.
 * @param parent The parent path.
 * @param name The name of the child dentry.
 * @param flags Flags for the path walk.
 * @param ns The namespace to access mountpoints.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t path_walk_single_step(path_t* outPath, const path_t* parent, const char* name, walk_flags_t flags,
    namespace_t* ns);

/**
 * @brief Traverse a pathname from a specified starting path.
 *
 * @param outPath The output path.
 * @param pathname The patname to traverse to.
 * @param start The path to start at if the pathname is relative.
 * @param flags Flags for the path walk.
 * @param ns The namespace to access mountpoints.
 * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
 */
uint64_t path_walk(path_t* outPath, const pathname_t* pathname, const path_t* start, walk_flags_t flags,
    namespace_t* ns);

/**
 * @brief Traverse a pathname to its parent and get the last component name.
 *
 * @param outPath The output parent path.
 * @param pathname The pathname to traverse.
 * @param start The path to start at if the pathname is relative.
 * @param outLastName The output last component name.
 * @param flags Flags for the path walk.
 * @param ns The namespace to access mountpoints.
 * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
 */
uint64_t path_walk_parent(path_t* outPath, const pathname_t* pathname, const path_t* start, char* outLastName,
    walk_flags_t flags, namespace_t* ns);

/**
 * @brief Convert a path to a pathname.
 *
 * The resulting pathname will be absolute.
 *
 * @param path The path to convert.
 * @param pathname The output pathname.
 * @return On success, `0`. On failure, returns `ERR` and `errno` is set.
 */
uint64_t path_to_name(const path_t* path, pathname_t* pathname);

static inline void path_defer_cleanup(path_t** path)
{
    if (*path != NULL)
    {
        path_put(*path);
    }
}

/** @} */
