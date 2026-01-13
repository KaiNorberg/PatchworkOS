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

// clang-format off
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
 * Paths can have flags appended at the end, these flags are parsed to determine the mode of the related operation.
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
 * | `create` | `c` | Create the file or directory if it does not exist. |
 * | `exclusive` | `e` | Will cause the open to fail if the file or directory already exists and `:create` is specified. |
 * | `parents`   | `p` | Create any parent directories if they do not exist when creating a file or directory. |
 * | `truncate` | `t` | Truncate the file to zero length if it already exists. |
 * | `directory` | `d` | Create or remove directories. All other operations will ignore this flag. |
 * | `recursive` | `R` | If removing a directory, remove all its contents recursively. If using `getdents()`, list contents recursively. | 
 * | `nofollow`  | `l` | Do not follow symbolic links. | 
 * | `private`   | `P` | Any files with this flag will be closed before a process starts executing. Any mounts with this flag will not be copied to a child namespace. | 
 * | `propagate`  | `g` | Propagate mounts and unmounts to child namespaces. | 
 * | `locked`    | `L` | Forbid unmounting this mount, useful for hiding directories or files. |
 *
 * For convenience, a single letter short form is also available as shown above, these single letter forms do not need
 * to be separated by colons, for example `/path/to/file:rwcte` is equivalent to
 * `/path/to/file:read:write:create:truncate:exclusive`.
 *
 * The parsed mode is the primary way to handle both the behaviour of vfs operations and permissions in the
 * kernel. For example, a file opened from within a directory which was bound with only read permissions will also have
 * read only permissions, even if the file itself would allow write permissions.
 *
 * If no permissions, i.e. read, write or execute, are specified, the default is to open with the maximum currently
 * allowed permissions.
 *
 * @{
 */
// clang-format on

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
    MODE_PARENTS = 1 << 7,
    MODE_TRUNCATE = 1 << 8,
    MODE_DIRECTORY = 1 << 9,
    MODE_RECURSIVE = 1 << 10,
    MODE_NOFOLLOW = 1 << 11,
    MODE_PRIVATE = 1 << 12,
    MODE_PROPAGATE = 1 << 13,
    MODE_LOCKED = 1 << 14,
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
 * @brief Maximum iterations to handle `..` in a path.
 *
 * This is to prevent infinite loops.
 */
#define PATH_MAX_DOTDOT 1000

/**
 * @brief Maximum iterations to handle symlinks in a path.
 *
 * This is to prevent infinite loops.
 */
#define PATH_MAX_SYMLINK 40

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
} pathname_t;

/**
 * @brief Initialize a pathname.
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
 * @brief Check if a path is empty.
 *
 * @param path The path to check.
 * @return true if the path is empty, false otherwise.
 */
#define PATH_IS_EMPTY(path) ((path).mount == NULL && (path).dentry == NULL)

/**
 * @brief Check if a path is valid.
 *
 * @param path The path to check.
 * @return true if the path is valid, false otherwise.
 */
#define PATH_IS_VALID(path) ((path) != NULL && (path)->mount != NULL && (path)->dentry != NULL)

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
 * @brief Walk a single path component.
 *
 * @param path The path to step from, will be updated to the new path, may be negative.
 * @param mode The mode to open the new path with.
 * @param name The name of the new path component.
 * @param ns The namespace to access mountpoints.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t path_step(path_t* path, mode_t mode, const char* name, namespace_t* ns);

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

/**
 * @brief Convert a mode to a string representation.
 *
 * The resulting string will be null terminated.
 *
 * @param mode The mode to convert.
 * @param out The output string buffer.
 * @param length The length of the output string buffer.
 * @return On success, the length of the resulting string, excluding the null terminator. On failure, `ERR` and `errno`
 * is set to:
 * - `EINVAL`: Invalid parameters.
 * - `ENAMETOOLONG`: The output buffer is too small.
 */
uint64_t mode_to_string(mode_t mode, char* out, uint64_t length);

/**
 * @brief Check and adjust mode permissions.
 *
 * If no permissions are set in the mode, it will be adjusted to have the maximum allowed permissions.
 *
 * @param mode The mode to check and adjust.
 * @param maxPerms The maximum allowed permissions.
 * @return On success, the adjusted mode. On failure, `ERR` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EACCES`: Requested permissions exceed maximum allowed permissions.
 */
uint64_t mode_check(mode_t* mode, mode_t maxPerms);

static inline void path_defer_cleanup(path_t** path)
{
    if (*path != NULL)
    {
        path_put(*path);
    }
}

/** @} */
