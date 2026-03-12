#pragma once

#include <kernel/utils/ref.h>

#include <alloca.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/fs.h>
#include <sys/status.h>

typedef struct irp irp_t;
typedef struct path path_t;
typedef struct binding binding_t;
typedef struct dentry dentry_t;
typedef struct file file_t;

// clang-format off
/**
 * @brief Unique location in the filesystem.
 * @defgroup kernel_fs_path Path
 * @ingroup kernel_fs
 *
 * A path represents a single unique location in the filesystem hierarchy.
 *
 * ## Flags
 *
 * Paths can have flags appended at the end, these flags are parsed to determine the mode of the related operation.
 *
 * Each flag starts with `:` and multiple instances of the same flag are allowed, for example
 * `/path/to/file:append:append:execute`.
 *
 * Included is a list of all available flags:
 *
 * | Flag | Short | Description |
 * |------|-------|-------------|
 * | `read`      | `r` | Open with read permissions. |
 * | `write`     | `w` | Open with write permissions. |
 * | `execute`   | `x` | Open with execute permissions. |
 * | `append`    | `a` | Any data written to the file will be appended to the end. |
 * | `create`      | `c` | Create a regular file, or fail if the file already exists but is not a regular file. |
 * | `directory` | `d` | Create a directory, or fail if the file already exists but is not a directory. |
 * | `symlink`   | `s` | Create a symlink, or fail if the file already exists but is not a symlink. |
 * | `hardlink`  | `h` | Create a hardlink, or fail if the file already exists. |
 * | `exclusive` | `e` | Will cause the open to fail if the file already exists. |
 * | `existing`  | `E` | Force failure if the file does not exist even if any creation flags are specified, useful if you want to, for example, ensure you are opening a directory. | 
 * | `truncate`  | `t` | Truncate the file to zero length if it already exists. | 
 * | `nofollow`  | `l` | Do not follow symlinks. | 
 * | `private`   | `P` | Any files with this flag will be closed before a process starts executing. Any mounts with this flag will not be copied to a child namespace. | 
 * | `parents`   | `p` | Create parent directories if they do not exist. |
 * | `locked`    | `L` | Forbid unmounting this binding, useful for hiding directories or files. |
 *
 * For convenience, a single letter short form is also available as shown above, these single letter forms do not need
 * to be separated by colons, for example `/path/to/file:rwfte` is equivalent to
 * `/path/to/file:read:write:file:truncate:exclusive`.
 *
 * The parsed mode is the primary way to handle both the behaviour of vfs operations and permissions in the
 * kernel. For example, a file opened from within a directory which was bound with only read permissions will also have
 * read only permissions, even if the file itself would allow write permissions.
 *
 * If no permissions, i.e. read, write or execute, are specified, the default is to open with the maximum currently
 * allowed permissions.
 *
 * ## Payload
 *
 * In addition to flags, a path can contain an optional payload, specified after a `?` at the end of the path. This is
 * used to pass additional data to the kernel for certain operations, for example, the file descriptor to create a
 * hardlink to as an integer when used with the `hardlink` flag.
 *
 * For a practical example, we can create a symlink using the path `/path/to/symlink:symlink?/path/to/target`.
 *
 * @note The payload itself is not parsed by the path parser, instead it only extracts it and passes it to the vnode
 * being opened as a `NULL`-terminated string, it is then responsible for parsing it and using it as needed. This means
 * that unique filesystems could define their own custom payload formats and semantics.
 *
 * ## Forbidden Characters
 *
 * The below characters are forbidden in paths:
 *
 * | Character | Description |
 * | 0 ... 31  | Control characters. |
 * | `<` | Less than. |
 * | `>` | Greater than. |
 * | `:` | Colon (reserved for flags). |
 * | `"` | Double quote. |
 * | `/` | Forward slash. |
 * | `\` | Backslash. |
 * | `|` | Pipe. |
 * | `?` | Question mark (reserved for payload). |
 * | `*` | Asterisk. |
 *
 * These characters are not coincidentally the same characters reserved by Windows NT. As such, using them is already
 * bad practice and their loss is not significant.
 *
 * @see https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file
 *
 * ## Rationale
 *
 * The primary intent behind the use of the flags and payload system is to allow for greater composability. With this
 * system, any environment that can open a file, a Lua script, a shell, etc. can create any file, directory, symlink or
 * hardlink with any permissions and flags without needing to rely on custom "PatchworkOS extensions".
 *
 * One can as an exercise imagine the potential of a basic "touch" shell utility with this system.
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
    MODE_READ = 1 << 0, ///< Handled by the VFS.
    MODE_WRITE = 1 << 1, ///< Handled by the VFS.
    MODE_EXECUTE = 1 << 2, ///< Handled by the VFS.
    MODE_APPEND = 1 << 3, ///< Should be implemented by the filesystem.
    /**
     * Handled by the VFS, if `MODE_DIRECTORY`, `MODE_SYMLINK` and `MODE_HARDLINK` are not specified, then a `IRP_MJ_CREATE` handler should create a regular file.
     */
    MODE_CREATE = 1 << 4,
    MODE_DIRECTORY = 1 << 5, ///< Specifies what to create in a `IRP_MJ_CREATE` handler.
    MODE_SYMLINK = 1 << 6, ///< Specifies what to create in a `IRP_MJ_CREATE` handler.
    MODE_HARDLINK = 1 << 7, ///< Specifies what to create in a `IRP_MJ_CREATE` handler.
    MODE_EXCLUSIVE = 1 << 8, ///< Handled by the VFS.
    MODE_EXISTING = 1 << 9, ///< Handled by the VFS.
    MODE_TRUNCATE = 1 << 10, ///< Should be implemented by the filesystem.
    MODE_NOFOLLOW = 1 << 11, ///< Handled by the VFS.
    MODE_PRIVATE = 1 << 12, ///< Handled by the VFS.
    MODE_PARENTS = 1 << 13, ///< Handled by the VFS.
    MODE_LOCKED = 1 << 14, ///< Handled by the VFS.
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
    binding_t* binding;
    dentry_t* dentry;
} path_t;

/**
 * @brief Helper to create an empty path.
 *
 * Its important to always use this as some functions, for example `path_copy()`, will deref the existing binding and
 * dentry in the path.
 *
 * @return An empty path.
 */
#define PATH_EMPTY \
    (path_t) \
    { \
        .binding = NULL, .dentry = NULL \
    }

/**
 * @brief Helper to create a path.
 *
 * @param inBinding The binding of the path.
 * @param inDentry The dentry of the path.
 * @return The created path.
 */
#define PATH_CREATE(inBinding, inDentry) \
    (path_t) \
    { \
        .binding = REF(inBinding), .dentry = REF(inDentry), \
    }

/**
 * @brief Check if a path is empty.
 *
 * @param path The path to check.
 * @return true if the path is empty, false otherwise.
 */
#define PATH_IS_EMPTY(path) ((path).binding == NULL && (path).dentry == NULL)

/**
 * @brief Check if a path is valid.
 *
 * @param path The path to check.
 * @return true if the path is valid, false otherwise.
 */
#define PATH_IS_VALID(path) ((path) != NULL && (path)->binding != NULL && (path)->dentry != NULL)

/**
 * @brief Set a path.
 *
 * Will deref the existing binding and dentry in the path if they are not `NULL`.
 *
 * @param path The path to set.
 * @param binding The binding to set.
 * @param dentry The dentry to set.
 */
static inline void path_set(path_t* path, binding_t* binding, dentry_t* dentry)
{
    if (dentry != NULL)
    {
        REF(dentry);
    }

    if (binding != NULL)
    {
        REF(binding);
    }

    if (path->dentry != NULL)
    {
        UNREF(path->dentry);
    }

    if (path->binding != NULL)
    {
        UNREF(path->binding);
    }

    path->dentry = dentry;
    path->binding = binding;
}

/**
 * @brief Copy a path.
 *
 * Will deref the existing binding and dentry in the destination path if they are not `NULL`.
 *
 * @param dest The destination path.
 * @param src The source path.
 */
static inline void path_copy(path_t* dest, const path_t* src)
{
    path_set(dest, src->binding, src->dentry);
}

/**
 * @brief Put a path.
 *
 * Will deref the binding and dentry in the path if they are not `NULL`.
 *
 * @param path The path to put.
 */
static inline void path_put(path_t* path)
{
    if (path->dentry != NULL)
    {
        UNREF(path->dentry);
        path->dentry = NULL;
    }

    if (path->binding != NULL)
    {
        UNREF(path->binding);
        path->binding = NULL;
    }
}

/**
 * @brief Path walking state.
 * @struct path_state_t
 *
 * @note The `dentry` and `binding` members will only hold references while outside of a RCU read-side critical section.
 * For example, during lookups. Within these section there is no need for reference counting, improving performance.
 */
typedef struct path_state
{
    dentry_t* dentry;      ///< The current dentry in the walk.
    binding_t* binding;    ///< The current binding in the walk.
    file_t* root;          ///< The root file, specifies the root path and bindings.
    char* ptr;             ///< Pointer to the current component in the path.
    size_t componentLen;   ///< Length of the current component being processed.
    mode_t mode;           ///< Parsed mode from the path.
    uint32_t symlinkDepth; ///< Current symlink recursion depth.
    dentry_t* lookup;      ///< A reference to the last "looked up" dentry to keep it and its parents alive.
    status_t (*done)(irp_t* irp, struct path_state* state, file_t* file);
    char* payload;             ///< The payload string extracted from the path.
    uint64_t count;            ///< The length of the path string.
    char path[MAX_PATH];       ///< The full path string buffer, not `NULL` terminated.
    char linkBuffer[MAX_PATH]; ///< Temporary buffer for reading symlinks.
} path_state_t;

/**
 * @brief Initialize a path state.
 *
 * After calling this function the path to walk should be copied to `path_state_t::path` and the length of the path
 * should be set in `path_state_t::count` before calling `path_walk()`.
 *
 * @param state The path state to initialize.
 */
static inline void path_state_init(path_state_t* state, dentry_t* dentry, binding_t* binding, file_t* root,
    status_t (*done)(irp_t* irp, struct path_state* state, file_t* file))
{
    state->dentry = dentry;
    state->binding = binding;
    state->root = root != NULL ? REF(root) : NULL;
    state->ptr = state->path;
    state->componentLen = 0;
    state->mode = MODE_NONE;
    state->symlinkDepth = 0;
    state->lookup = NULL;
    state->done = done;
    state->payload = NULL;
    state->count = 0;
    state->path[0] = '\0';
    state->linkBuffer[0] = '\0';
}

/**
 * @brief Asynchronously walk a path.
 *
 * @param irp The IRP to use for the operation.
 * @param state The walking state, must be allocated by the caller but will be freed upon completion.
 * @return An appropriate status value.
 */
status_t path_walk(irp_t* irp, path_state_t* state);

/**
 * @brief Convert a path to a pathname.
 *
 * The resulting pathname will be absolute.
 *
 * @param path The path to convert.
 * @param pathname The output pathname.
 * @param length The length of the output pathname buffer.
 * @return An appropriate status value.
 */
status_t path_to_name(const path_t* path, char* pathname, size_t length);

/**
 * @brief Convert a mode to a string representation.
 *
 * The resulting string will be null terminated.
 *
 * @param mode The mode to convert.
 * @param out The output string buffer.
 * @param length The length of the output string buffer.
 * @param outLength Output pointer to store the length of the resulting string, excluding the null terminator.
 * @return An appropriate status value.
 */
status_t mode_to_string(mode_t mode, char* out, uint64_t length, uint64_t* outLength);

/**
 * @brief Check and adjust mode permissions.
 *
 * If no permissions are set in the mode, it will be adjusted to have the maximum allowed permissions.
 *
 * @param mode The mode to check and adjust.
 * @param maxPerms The maximum allowed permissions.
 * @return An appropriate status value.
 */
status_t mode_check(mode_t* mode, mode_t maxPerms);

static inline void path_defer_cleanup(path_t** path)
{
    if (*path != NULL)
    {
        path_put(*path);
    }
}

/** @} */
