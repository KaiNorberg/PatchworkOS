#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <stdarg.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_AUX/ERR.h"
#include "_AUX/MAX_NAME.h"
#include "_AUX/MAX_PATH.h"
#include "_AUX/NULL.h"
#include "_AUX/SEEK.h"
#include "_AUX/clock_t.h"
#include "_AUX/config.h"
#include "_AUX/fd_t.h"

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define PIPE_READ 0
#define PIPE_WRITE 1

/**
 * @brief Standard library IO extension.
 * @ingroup libstd
 * @defgroup libstd_sys_io System IO
 *
 * The `sys/io.h` header handles interaction with PatchworkOS's file system,
 * following the philosophy that **everything is a file**. This means interacting with physical devices,
 * inter-process communication (like shared memory), and much more is handled via files.
 *
 * ### File Paths and Flags
 *
 * PatchworkOS has a multiroot file system using labels not letters. You may also notice that functions like `open` do
 * not have a specific argument for flags, instead the filepath itself contains the flags. This is done by appending a
 * '?' char to the end of a file path and then separating flags using the '&' char, like a web link. This system makes
 * using a shell far simpler and powerful as you can add flags to your filepath directly without any additional handling
 * by the shell, for example there is no need for a "truncate" redirect (>>) instead you can just add the "trunc" flag
 * to the filepath and use a normal redirect (>).
 *
 * Here is an example filepath: `this:/is/a/path?with&some&flags`.
 *
 * #### Flags
 *
 * Below are all the currently implemented flags. Note that not all files will have all flags implemented.
 *
 * - `nonblock` - Prevents the file from blocking, if a block is supposed to occur a error will take place and errno is
 * set to EWOULDBLOCK.
 * - `append` - Causes the file to move its offset to the end of the file before each write atomically.
 * - `create` - If the filepath does not exist, create it. Without this attempting to open a non existent filepath will
 * return an error.
 * - `excl` - If used with `create` then it guarantees that a file will be created, if a file already exists an error
 * occurs.
 * - `trunc` - If the file already exists then truncate its length to 0.
 * - `dir` - Should be used when opening a directory.
 *
 * ### Default Volumes
 *
 * There are two volumes mounted by default.
 * - `home:/` - Stores the bootloader, kernel, programs and other user data, it's the equivalent to the C: drive in
 * DOS-like systems.
 * - `sys:/` - Stores system resources, for example `sys:/kbd/ps2`, it's the equivalent of the `dev`, `proc` and `sys`
 * folders on Unix-like systems.
 */

/**
 * @brief System call for opening files.
 * @ingroup libstd_sys_io
 *
 * The `open` function opens a file located at a given path.
 *
 * @param path The path to the desired file.
 * @return On success, returns the file descriptor, on failure returns `ERR` and errno is set.
 */
fd_t open(const char* path);

/**
 * @brief Wrapper for opening files with a formatted path.
 * @ingroup libstd_sys_io
 *
 * The `openf` function opens a file located at a path specified by a format string and variable arguments. This is very
 * usefull considering the amount of file path processing required for many of Patchworks APIs.
 *
 * @param format The format string specifying the path to the desired file.
 * @param ... Variable arguments to be formatted into the path string.
 * @return On success, returns the file descriptor, on failure returns `ERR` and errno is set.
 */
fd_t openf(const char* _RESTRICT format, ...);

/**
 * @brief Wrapper for opening files with a formatted path, using va_list.
 * @ingroup libstd_sys_io
 *
 * The `vopenf` function is similar to `openf` but takes a `va_list` for the arguments.
 *
 * @param format The format string specifying the path to the desired file.
 * @param args A `va_list` containing the arguments to be formatted into the path string.
 * @return On success, returns the file descriptor, on failure returns `ERR` and errno is set.
 */
fd_t vopenf(const char* _RESTRICT format, va_list args);

/**
 * @brief System call for opening 2 file descriptors from one file.
 * @ingroup libstd_sys_io

 * The `open2` function opens a file and returns two file descriptors. This is intended as a more generic implementation
 * of system calls like pipe() in for example Linux. One example use case of this system call is as expected pipes, if
 * `open2` is called on `sys:/pipe/new` then `fd[0]` will store the read end of the pipe and `fd[1]` will store the
 write
 * end of the pipe. But if open() where to be called on `sys:/pipe/new` then the returned file descriptor would be both
 * ends.
 *
 * @param path The path to the desired file.
 * @param fd An array of two `fd_t` where the new file descriptors will be stored.
 * @return On success, returns 0, on failure returns `ERR` and errno is set.
 */
uint64_t open2(const char* path, fd_t fd[2]);

/**
 * @brief System call for closing files.
 * @ingroup libstd_sys_io

 * The `close` function closes an open file descriptor.
 *
 * @param fd The file descriptor to close.
 * @return On success, returns 0, on failure returns `ERR` and errno is set.
 */
uint64_t close(fd_t fd);

/**
 * @brief  System call for reading from files.
 * @ingroup libstd_sys_io

 * The `read` function reads data from a file descriptor into a buffer.
 *
 * @param fd The file descriptor to read from.
 * @param buffer A pointer to the buffer where the data will be stored.
 * @param count The maximum number of bytes to read.
 * @return On success, returns the number of bytes read. On end-of-file, returns 0. On failure, returns `ERR` and errno
 * is set.
 */
uint64_t read(fd_t fd, void* buffer, uint64_t count);

/**
 * @brief System call for writing to files.
 * @ingroup libstd_sys_io
 *
 * The `write` function writes data from a buffer to a file descriptor.
 *
 * @param fd The file descriptor to write to.
 * @param buffer A pointer to the buffer containing the data to write.
 * @param count The number of bytes to write.
 * @return On success, returns the number of bytes written. On failure, returns `ERR` and errno is set.
 */
uint64_t write(fd_t fd, const void* buffer, uint64_t count);

/**
 * @brief Wrapper for writing a formatted string to a file.
 * @ingroup libstd_sys_io
 *
 * The `writef` function writes formatted output to a file descriptor.
 *
 * @param fd The file descriptor to write to.
 * @param format The format string.
 * @param ... Variable arguments to be formatted.
 * @return On success, returns the number of bytes written. On failure, returns `ERR` and errno is set.
 */
uint64_t writef(fd_t fd, const char* _RESTRICT format, ...);

/**
 * @brief Write formatted with va_list system call.
 * @ingroup libstd_sys_io
 *
 * The `vwritef` function is similar to `writef` but takes a `va_list` for the arguments.
 *
 * @param fd The file descriptor to write to.
 * @param format The format string.
 * @param args A `va_list` containing the arguments to be formatted.
 * @return On success, returns the number of bytes written. On failure, returns `ERR` and errno is set.
 */
uint64_t vwritef(fd_t fd, const char* _RESTRICT format, va_list args);

/**
 * @brief
 *
 */
typedef uint8_t seek_origin_t;

/**
 * @brief System call for changing the file offset.
 * @ingroup libstd_sys_io
 *
 * The `seek` function moves the offset of the open file associated with the file descriptor `fd`.
 *
 * @param fd The file descriptor.
 * @param offset The offset to move the file pointer.
 * @param origin The origin that the offset is relative to (e.g., `SEEK_SET`, `SEEK_CUR`, `SEEK_END`).
 * @return On success, returns the new offset from the beginning of the file. On failure, returns `ERR` and errno is
 * set.
 */
uint64_t seek(fd_t fd, int64_t offset, seek_origin_t origin);

/**
 * @brief System call for changing the cwd.
 * @ingroup libstd_sys_io
 *
 * The `chdir` function changes the current working directory of the calling process.
 *
 * @param path The path to the new directory.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t chdir(const char* path);

typedef enum poll_event
{
    POLL_READ = (1 << 0),
    POLL_WRITE = (1 << 1),
    POLL1_ERR = (1 << 31)
} poll_event_t;

typedef struct pollfd
{
    fd_t fd;
    poll_event_t events;
    poll_event_t revents;
} pollfd_t;

/**
 * @brief System call for polling files.
 * @ingroup libstd_sys_io
 *
 * The `poll` function waits for one of a set of file descriptors to become ready to perform I/O.
 *
 * @param fds An array of `pollfd_t` structures, each specifying a file descriptor to poll in pollfd_t::fd and the
 * events to wait for in pollfd_t::events.
 * @param amount The number of `pollfd_t` structures in the `fds` array.
 * @param timeout The maximum time (in clock ticks) to wait for an event. If `CLOCKS_NEVER`, it waits forever.
 * @return On success, returns the number of file descriptors for which the events occurred. On timeout, returns 0. On
 * failure, returns `ERR` and errno is set.
 */
uint64_t poll(pollfd_t* fds, uint64_t amount, clock_t timeout);

/**
 * @brief Wrapper for polling one file.
 * @ingroup libstd_sys_io
 *
 * The `poll1` function waits for events on a single file descriptor. Otherwise it is identical to `poll` and exists
 * simply for convenience.
 *
 * @param fd The file descriptor to poll.
 * @param events The events to wait for (e.g., `POLL_READ`, `POLL_WRITE`).
 * @param timeout The maximum time (in clock ticks) to wait for an event. If `CLOCKS_NEVER`, it waits forever.
 * @return On success, returns the events that revents. On timeout, returns 0. On failure, the `POLL1_ERR` event bit is
 * set and errno is set.
 */
poll_event_t poll1(fd_t fd, poll_event_t events, clock_t timeout);

typedef enum stat_type
{
    STAT_FILE = 0,
    STAT_DIR = 1,
} stat_type_t;

typedef struct stat
{
    char name[MAX_NAME];
    stat_type_t type;
    uint64_t size;
} stat_t;

/**
 * @brief System call for retrieving info about a file or directory.
 * @ingroup libstd_sys_io
 *
 * The `stat` function retrieves information about a file or directory.
 *
 * @param path The path to the file or directory.
 * @param stat A pointer to a `stat_t` structure where the file information will be stored.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t stat(const char* path, stat_t* stat);

/**
 * @brief System call for extended driver behaviour.
 * @ingroup libstd_sys_io
 *
 * The `ioctl` function allows drivers to implement unusual behaviour that would be impossible or impractical with a
 * normal file-based API.
 *
 * @param fd The file descriptor of the file.
 * @param request The driver-dependent request code.
 * @param argp A pointer to an argument that depends on the request.
 * @param size The size of the argument pointed to by `argp`.
 * @return On success, the return value depends on the driver but is usually 0. On failure, returns `ERR` and errno is
 * set.
 */
uint64_t ioctl(fd_t fd, uint64_t request, void* argp, uint64_t size);

/**
 * @brief System call for duplicating file descriptors.
 * @ingroup libstd_sys_io
 *
 * The `dup` function duplicates an open file descriptor. The new file descriptor refers to the same open file
 * description as the old file descriptor.
 *
 * @param oldFd The open file descriptor to duplicate.
 * @return On success, returns the new file descriptor. On failure, returns `ERR` and errno is set.
 */
fd_t dup(fd_t oldFd);

/**
 * @brief System call for duplicating file descriptors, with a destination.
 * @ingroup libstd_sys_io
 *
 * The `dup2` function duplicates an open file descriptor to a specified new file descriptor. If `newFd` is already
 * open, it will be closed atomically.
 *
 * @param oldFd The open file descriptor to duplicate.
 * @param newFd The desired new file descriptor.
 * @return On success, returns the new file descriptor. On failure, returns `ERR` and errno is set.
 */
fd_t dup2(fd_t oldFd, fd_t newFd);

/**
 * @brief System call for reading directory entires.
 * @ingroup libstd_sys_io
 *
 * The `readdir` function reads information about every entry in a directory from a directory file descriptor. Think of
 * it like calling stat() on everything in the directory. The intention is to call `readdir` twice once to get the total
 * number of entires in the directory allowing you to allocate a buffer of the correct size, then again to read the
 * entires, ideally this would be done in a loop to make sure the amount of entires does not change between calls to
 * `readdir`.
 *
 * @param fd The file descriptor of the directory to read.
 * @param infos A pointer to an array of `stat_t` structures where the directory entry information will be stored.
 * @param amount The amount of `stat_t` structures that fit in the infos array.
 * @return On success, returns the total number of entries that exists, NOT the amount of entires read. On failure,
 * returns `ERR` and errno is set.
 */
uint64_t readdir(fd_t fd, stat_t* infos, uint64_t amount);

typedef struct
{
    uint64_t amount;
    stat_t infos[];
} allocdir_t;

/**
 * @brief Wrapper for easily reading directory entries.
 * @ingroup libstd_sys_io
 *
 * The `allocdir` function reads all directory entries from a directory file descriptor and allocates memory for them.
 *
 * @param fd The file descriptor of the directory to read.
 * @return On success, returns a pointer to an `allocdir_t` structure containing the number of entries and an array of
 * `stat_t` structures. On failure, returns `NULL` and errno is set.
 */
allocdir_t* allocdir(fd_t fd);

/**
 * @brief Wrapper for creating a directory.
 * @ingroup libstd_sys_io
 *
 * The `mkdir` function creates a new directory.
 *
 * @param path The path of the directory to create.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t mkdir(const char* path);

#if defined(__cplusplus)
}
#endif

#endif
