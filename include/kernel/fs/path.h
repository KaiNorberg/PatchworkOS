#pragma once

#include <kernel/utils/map.h>

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
 * mountpoints. A dentry can exist in a filesystem that is mounted at multiple locations in the filesystem hierarchy,
 * thus both a mountpoint and a dentry is needed to uniquely identify a location.
 *
 * ## Flags/Mode
 *
 * Paths can have flags appended at the end, these flags are parsed to determine the mode with which the path is opened.
 *
 * Each flag starts with `:` and multiple instances of the same flag are allowed, for example
 * `/path/to/file:append:append:nonblock`.
 *
 * Included is a list of all available flags:
 *
 * | Flag | Short | Description |
 * |------|-------|-------------|
 * | `read` | `r` | Open with read permissions. |
 * | `write` | `w` | Open with write permissions. |
 * | `execute` | `x` | Open with execute permissions. |
 * | `nonblock` | `n` | The file will not block on operations that would normally block. |
 * | `append` | `a` | Any data written to the file will be appended to the end. |
 * | `create` | `c` | Create the file if it does not exist. |
 * | `exclusive` | `e` | Will cause the open to fail if the file already exists. |
 * | `truncate` | `t` | Truncate the file to zero length if it already exists. |
 * | `directory` | `d` | Allow opening directories. |
 * | `recursive` | `R` | Behaviour differs, but allows for recursive operations, for example when used with `remove` it
 * will remove directories and their children recursively. |
 *
 * For convenience, a single letter short form is also available as shown above, these single letter forms do not need
 * to be separated by colons, for example `/path/to/file:rwcte` is equivalent to
 * `/path/to/file:read:write:create:truncate:exclusive`.
 *
 * The parsed mode is the primary way to handle both the behaviour of opened paths and permissions through out the
 * kernel. For example, a file opened from within a directory which was bound with only read permissions will also have
 * read only permissions, even if the file itself would allow write permissions.
 *
 * If no permissions, i.e. read, write or execute, are specified, the default is to open with the maximum currently
 * allowed permissions.
 *
 * @see kernel_fs_namespace for information on mode inheritance when binding paths.
 *
 * @{
 */

/**
 * @brief Path flags and permissions.
 * @enum mode_t
 *
 * We store both flags and permissions in the same enum but permissions are sometimes treated differently to flags.
 */
typedef enum mode
{
    MODE_NONE = 0,
    MODE_READ = 1 << 0,
    MODE_WRITE = 1 << 1,
    MODE_EXECUTE = 1 << 2,
    MODE_NONBLOCK = 1 << 3,
    MODE_APPEND = 1 << 4,
    MODE_CREATE = 1 << 5,
    MODE_EXCLUSIVE = 1 << 6,
    MODE_TRUNCATE = 1 << 7,
    MODE_DIRECTORY = 1 << 8,
    MODE_RECURSIVE = 1 << 9,
    MODE_AMOUNT = 10,
    MODE_ALL_PERMS = MODE_READ | MODE_WRITE | MODE_EXECUTE,
} mode_t;

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
 * @todo Replace with array lookup.
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
    mode_t mode;
    bool isValid;
} pathname_t;

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
 * @return On success, `0`. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENAMETOOLONG`: The string is too long or a component name is too long.
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
 * @brief Walk a single step in a path.
 *
 * @param path The path to traverse, will be updated to the new path, may be negative.
 * @param name The name of the child dentry.
 * @param ns The namespace to access mountpoints.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t path_step(path_t* path, const char* name, namespace_t* ns);

/**
 * @brief Walk a pathname to a path.
 *
 * @param path The path to start from, will be updated to the new path, may be negative.
 * @param pathname The pathname to walk to.
 * @param ns The namespace to access mountpoints.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t path_walk(path_t* path, const pathname_t* pathname, namespace_t* ns);

/**
 * @brief Walk a pathname to its parent and get the name of the last component.
 *
 * Will not modify `outParent` and `outChild` on failure.

 * @param path The path to start from, will be updated to the parent path.
 * @param pathname The pathname to traverse.
 * @param outLastName The output last component name, must be at least `MAX_NAME` bytes.
 * @param ns The namespace to access mountpoints.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t path_walk_parent(path_t* path, const pathname_t* pathname, char* outLastName, namespace_t* ns);

/**
 * @brief Traverse a pathname to its parent and child paths.
 *
 * Will not modify `outParent` and `outChild` on failure.
 *
 * @param from The path to start from.
 * @param outParent The output parent path.
 * @param outChild The output child path, may be negative.
 * @param pathname The pathname to traverse.
 * @param ns The namespace to access mountpoints.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t path_walk_parent_and_child(const path_t* from, path_t* outParent, path_t* outChild, const pathname_t* pathname,
    namespace_t* ns);

/**
 * @brief Convert a path to a pathname.
 *
 * The resulting pathname will be absolute.
 *
 * @param path The path to convert.
 * @param pathname The output pathname.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
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
