#ifndef _SYS_FS_H
#define _SYS_FS_H 1

#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/status.h>
#include <sys/syscall.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_libstd/MAX_NAME.h"
#include "_libstd/MAX_PATH.h"
#include "_libstd/NULL.h"
#include "_libstd/clock_t.h"
#include "_libstd/config.h"
#include "_libstd/fd_t.h"
#include "_libstd/ssize_t.h"
#include "_libstd/time_t.h"

/**
 * @brief Filesystem header.
 * @defgroup libstd_sys_fs Filesystem IO
 * @ingroup libstd
 *
 * @{
 */

#define STDIN_FILENO 0  ///< Standard input file descriptor.
#define STDOUT_FILENO 1 ///< Standard output file descriptor.
#define STDERR_FILENO 2 ///< Standard error file descriptor.

/**
 * @brief Maximum buffer size for the `F()` macro.
 */
#define F_MAX_SIZE 512

/**
 * @brief Allocates a formatted string on the stack.
 *
 * @warning Will terminate the program if the size of the formatted string is too large or if an encoding error occurs.
 */
#define F(format, ...) \
    ({ \
        char* _buffer = alloca(F_MAX_SIZE); \
        int _len = snprintf(_buffer, F_MAX_SIZE, format, __VA_ARGS__); \
        if (_len < 0 || _len >= F_MAX_SIZE) \
        { \
            abort(); \
        } \
        _buffer; \
    })

/**
 * @brief System call for opening files.
 *
 * The `open()` function opens a file located at a given path.
 *
 * @param out Output pointer for the opened file descriptor.
 * @param path The path to the desired file.
 * @return An appropriate status value.
 */
static inline status_t open(fd_t* out, const char* path)
{
    return syscall1(SYS_OPEN, out, (uintptr_t)path);
}

/**
 * @brief System call for opening files relative to another file descriptor.
 *
 * @param out Output pointer for the opened file descriptor.
 * @param from The file descriptor to open the file relative to, or `FD_NONE` to open from the current working
 * directory.
 * @param path The path to the desired file.
 * @return An appropriate status value.
 */
static inline status_t openat(fd_t* out, fd_t from, const char* path)
{
    return syscall2(SYS_OPENAT, out, from, (uintptr_t)path);
}

/**
 * @brief System call for closing files.
 *
 * @param fd The file descriptor to close.
 * @return An appropriate status value.
 */
static inline status_t close(fd_t fd)
{
    return syscall1(SYS_CLOSE, NULL, fd);
}

/**
 * @brief Wrapper for reading a file directly into a null-terminated string.
 *
 * The `reads()` function reads the entire contents of a file into a newly allocated null-terminated string.
 * The caller is responsible for freeing the returned string.
 *
 * @param out Output pointer for the null-terminated string.
 * @param fd The file descriptor to read from.
 * @return An appropriate status value.
 */
status_t reads(char** out, fd_t fd);

/**
 * @brief Wrapper for reading a file directly using a path.
 *
 * Equivalent to calling `open()`, `seek()`, `read()`, and `close()` in sequence.
 *
 * @param path The path to the file.
 * @param buffer A pointer to the buffer where the data will be stored.
 * @param count The maximum number of bytes to read.
 * @param offset The offset in the file to start reading from.
 * @param bytesRead Output pointer for the number of bytes read.
 * @return An appropriate status value.
 */
status_t readfile(const char* path, void* buffer, size_t count, size_t offset, size_t* bytesRead);

/**
 * @brief Wrapper for reading an entire file directly into a null-terminated string.
 *
 * The `readfiles()` function reads the entire contents of a file into a newly allocated null-terminated string.
 * The caller is responsible for freeing the returned string.
 *
 * Equivalent to calling `open()`, `reads()`, and `close()` in sequence.
 *
 * @param out Output pointer for the null-terminated string.
 * @param path The path to the file.
 * @return An appropriate status value.
 */
status_t readfiles(char** out, const char* path);

/**
 * @brief Wrapper for writing a null-terminated string to a file.
 *
 * @param fd The file descriptor to write to.
 * @param string The null-terminated string to write.
 * @param bytesWritten Output pointer for the number of bytes written.
 * @return An appropriate status value.
 */
status_t writes(fd_t fd, const char* string, size_t* bytesWritten);

/**
 * @brief Wrapper for writing to a file directly using a path.
 *
 * Equivalent to calling `open()`, `seek()`, `write()`, and `close()` in sequence.
 *
 * @param path The path to the file.
 * @param buffer A pointer to the buffer containing the data to write.
 * @param count The number of bytes to write.
 * @param offset The offset in the file to start writing to.
 * @param bytesWritten Output pointer for the number of bytes written.
 * @return An appropriate status value.
 */
status_t writefile(const char* path, const void* buffer, size_t count, size_t offset, size_t* bytesWritten);

/**
 * @brief Wrapper for writing a null-terminated string directly to a file using a path.
 *
 * Equivalent to calling `open()`, `writes()`, and `close()` in sequence.
 *
 * @param path The path to the file.
 * @param string The null-terminated string to write.
 * @return An appropriate status value.
 */
status_t writefiles(const char* path, const char* string);

/**
 * @brief Wrapper for reading from a file descriptor using scan formatting.
 *
 * @param fd The file descriptor to read from.
 * @param format The format string.
 * @return The number of input items successfully matched and assigned.
 */
uint64_t scan(fd_t fd, const char* format, ...);

/**
 * @brief Wrapper for reading from a file descriptor using scan formatting with `va_list`.
 *
 * @param fd The file descriptor to read from.
 * @param format The format string.
 * @param args The va_list of arguments.
 * @return The number of input items successfully matched and assigned.
 */
uint64_t vscan(fd_t fd, const char* format, va_list args);

/**
 * @brief Wrapper for reading from a file path using scan formatting.
 *
 * Equivalent to calling `open()`, `scan()`, and `close()` in sequence.
 *
 * @param path The path to the file.
 * @param format The format string.
 * @return The number of input items successfully matched and assigned.
 */
uint64_t scanfile(const char* path, const char* format, ...);

/**
 * @brief Wrapper for reading from a file path using scan formatting with `va_list`.
 *
 * Equivalent to calling `open()`, `vscan()`, and `close()` in sequence.
 *
 * @param path The path to the file.
 * @param count Output pointer for the number of input items successfully matched and assigned.
 * @param format The format string.
 * @param args The va_list of arguments.
 * @return The number of input items successfully matched and assigned.
 */
uint64_t vscanfile(const char* path, const char* format, va_list args);

/**
 * @brief System call for changing the cwd.
 *
 * @param path The path to the new directory.
 * @return On success, `0`. On failure, `-1`.
 */
int chdir(const char* path);

/**
 * @brief Vnode type enum.
 * @enum vnode_type_t
 */
typedef enum
{
    VNODE_NONE = 0, ///< Invalid vnode type.
    VNODE_REGULAR,  ///< Is a regular file.
    VNODE_DIR,      ///< Is a directory.
    VNODE_SYMLINK,  ///< Is a symbolic link.
} vnode_type_t;

/**
 * @brief A suberblock identifier that uniquely identifies a volume within the system.
 *
 * When combined with a vnode number, this can uniquely identify an vnode within the entire system.
 */
typedef uint64_t volume_id_t;

/**
 * @brief Stat type.
 * @struct stat_t
 */
typedef struct
{
    volume_id_t sbid;     ///< The volume ID of the filesystem containing the entry.
    uint64_t number;      ///< The number of the entries vnode.
    vnode_type_t type;    ///< The type of the entries vnode.
    uint64_t size;        ///< The size of the file that is visible outside the filesystem.
    uint64_t blocks;      ///< The amount of blocks used on disk to store the file.
    uint64_t blockSize;   ///< The preferred block size of the filesystem.
    uint64_t maxFileSize; ///< The maximum size of a file on this filesystem.
    uint64_t linkAmount;  ///< The amount of times the vnode appears in dentries.
    time_t accessTime;    ///< Unix time stamp for the last vnode access.
    time_t modifyTime;    ///< Unix time stamp for last file content alteration.
    time_t changeTime;    ///< Unix time stamp for the last file metadata alteration.
    time_t createTime;    ///< Unix time stamp for the creation of the vnode.
    char name[MAX_PATH];  ///< The name of the entry, not the full filepath. Includes the flags of the paths mount.
    uint8_t padding[64];  ///< Padding to leave space for future expansion.
} stat_t;

#ifdef static_assert
static_assert(sizeof(stat_t) == 416, "invalid stat_t size");
#endif

/**
 * @brief System call for retrieving info about a file or directory.
 *
 * @param path The path to the file or directory.
 * @param stat A pointer to a `stat_t` structure where the file information will be stored.
 * @return An appropriate status value.
 */
static inline status_t stat(const char* path, stat_t* stat)
{
    return syscall2(SYS_STAT, NULL, (uint64_t)path, (uint64_t)stat);
}

/**
 * @brief System call for creating a hardlink.
 *
 * @param oldPath
 * @param newPath
 * @return An appropriate status value.
 */
static inline status_t link(const char* oldPath, const char* newPath)
{
    return syscall2(SYS_LINK, NULL, (uintptr_t)oldPath, (uintptr_t)newPath);
}

/**
 * @brief System call for reading the target of a symbolic link.
 *
 * @param path The path to the symbolic link.
 * @param buffer A buffer to store the target path.
 * @param count The size of the buffer.
 * @param bytesRead Output pointer for the number of bytes read.
 * @return An appropriate status value.
 */
static inline status_t readlink(const char* path, char* buffer, uint64_t count, size_t* bytesRead)
{
    return syscall3(SYS_READLINK, bytesRead, (uintptr_t)path, (uintptr_t)buffer, count);
}

/**
 * @brief System call for creating a symbolic link.
 *
 * @param target The target path of the symbolic link.
 * @param linkpath The path where the symbolic link will be created.
 * @return An appropriate status value.
 */
static inline status_t symlink(const char* target, const char* linkpath)
{
    return syscall2(SYS_SYMLINK, NULL, (uintptr_t)target, (uintptr_t)linkpath);
}

/**
 * @brief System call for duplicating file descriptors.
 *
 * @param oldFd The open file descriptor to duplicate.
 * @param newFd Output pointer for the new file descriptor, if `FD_NONE` any free file descriptor will be used,
 * otherwise the specified file descriptor will be used.
 * @return An appropriate status value.
 */
static inline status_t dup(fd_t oldFd, fd_t* newFd)
{
    return syscall2(SYS_DUP, newFd, oldFd, *newFd);
}

/**
 * @brief Directory entry flags.
 * @enum dirent_flags_t
 */
typedef enum
{
    DIRENT_NONE = 0,
    DIRENT_MOUNTED = 1 << 0, ///< The directory entry is a mountpoint.
} dirent_flags_t;

/**
 * @brief Directory entry struct.
 *
 */
typedef struct
{
    vnode_type_t type;
    dirent_flags_t flags;
    char path[MAX_PATH]; ///< The relative path of the entry.
    char mode[MAX_PATH]; ///< The flags of the paths mount.
} dirent_t;

/**
 * @brief System call for reading directory entires.
 *
 * @param fd The file descriptor of the directory to read.
 * @param buffer The destination buffer.
 * @param count The size of the buffer in bytes.
 * @param bytesWritten Output pointer for the number of bytes written.
 * @return An appropriate status value.
 */
static inline status_t getdents(fd_t fd, dirent_t* buffer, uint64_t count, size_t* bytesWritten)
{
    return syscall3(SYS_GETDENTS, bytesWritten, fd, (uintptr_t)buffer, count);
}

/**
 * @brief Helper for reading all directory entries.
 *
 * The caller is responsible for freeing the returned pointer.
 *
 * @param fd The file descriptor of the directory to read.
 * @param buffer Output pointer to store the allocated buffer containing the directory entries.
 * @param count Output pointer to store the number of entries read.
 * @return An appropriate status value.
 */
status_t readdir(fd_t fd, dirent_t** buffer, uint64_t* count);

/**
 * @brief Wrapper for creating a directory.
 *
 * @param path The path of the directory to create.
 * @return On success, `0`. On failure, `EOF`.
 */
int mkdir(const char* path);

/**
 * @brief Wrapper for removing a directory.
 *
 * @param path The path of the directory to remove.
 * @return On success, `0`. On failure, `EOF`.
 */
int rmdir(const char* path);

/**
 * @brief Wrapper for removing a file.
 *
 * @param path The path of the file to remove.
 * @return An appropriate status value.
 */
status_t unlink(const char* path);

#define KEY_MAX 128 ///< Maximum size of a key generated by `share()`.

#define KEY_128BIT 25 ///< The size of a buffer needed to hold a 128-bit key.

#define KEY_256BIT 45 ///< The size of a buffer needed to hold a 256-bit key.

#define KEY_512BIT 89 ///< The size of a buffer needed to hold a 512-bit key.

/**
 * @brief System call for sharing a file descriptor with another process.
 *
 * @param key Output buffer to store the generated key.
 * @param size The size of the output buffer.
 * @param fd The file descriptor to share.
 * @param timeout The time until the shared file descriptor expires. If `CLOCKS_NEVER`, it never expires.
 * @return An appropriate status value.
 */
static inline status_t share(char* key, size_t size, fd_t fd, clock_t timeout)
{
    return syscall4(SYS_SHARE, NULL, (uintptr_t)key, size, fd, timeout);
}

/**
 * @brief Helper for sharing a file by its path.
 *
 * @param key Output buffer to store the generated key.
 * @param size The size of the output buffer.
 * @param path The path to the file to share.
 * @param timeout The time until the shared file descriptor expires. If `CLOCKS_NEVER`, it never expires.
 * @return An appropriate status value.
 */
static inline status_t sharefile(char* key, uint64_t size, const char* path, clock_t timeout)
{
    fd_t fd;
    status_t status = open(&fd, path);
    if (IS_ERR(status))
    {
        return status;
    }
    status = share(key, size, fd, timeout);
    close(fd);
    return status;
}

/**
 * @brief System call for claiming a shared file descriptor.
 *
 * After claiming a shared file descriptor, the key is no longer valid and cannot be used again.
 *
 * @param out Output pointer to store the claimed file descriptor.
 * @param key The key identifying the shared file descriptor.
 * @return An appropriate status value.
 */
static inline status_t claim(fd_t* out, const char* key)
{
    return syscall1(SYS_CLAIM, out, (uintptr_t)key);
}

/**
 * @brief System call for mounting a filesystem.
 *
 * @param mountpoint The target path to mount to.
 * @param fs The path to the desired filesystem in the `fs` sysfs directory.
 * @param options A string containing filesystem defined `key=value` pairs, with multiple options separated by commas,
 * or `NULL`.
 * @return An appropriate status value.
 */
static inline status_t mount(const char* mountpoint, const char* fs, const char* options)
{
    return syscall3(SYS_MOUNT, NULL, (uintptr_t)mountpoint, (uintptr_t)fs, (uintptr_t)options);
}

/**
 * @brief System call for unmounting a filesystem.
 *
 * @param mountpoint The target path to unmount.
 * @return An appropriate status value.
 */
static inline status_t unmount(const char* mountpoint)
{
    return syscall1(SYS_UNMOUNT, NULL, (uintptr_t)mountpoint);
}

/**
 * @brief System call for binding a file descriptor to a mountpoint.
 *
 * The created mount will inherit permissions from the source while the mount behaviour will follow the flags specified
 * in `mountpoint`.
 *
 * @param mountpoint The mountpoint path.
 * @param source The file descriptor to bind, must represent a directory.
 * @return An appropriate status value.
 */
static inline status_t bind(const char* mountpoint, fd_t source)
{
    return syscall2(SYS_BIND, NULL, (uintptr_t)mountpoint, source);
}

/** @} */

#if defined(__cplusplus)
}
#endif

#endif