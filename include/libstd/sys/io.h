#ifndef _SYS_IO_H
#define _SYS_IO_H 1

#include <stdarg.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C"
{
#endif

#include "_internal/ERR.h"
#include "_internal/MAX_NAME.h"
#include "_internal/MAX_PATH.h"
#include "_internal/NULL.h"
#include "_internal/SEEK.h"
#include "_internal/clock_t.h"
#include "_internal/config.h"
#include "_internal/fd_t.h"

/**
 * @brief System IO header.
 * @ingroup libstd
 * @defgroup libstd_sys_io System IO
 *
 * The `sys/io.h` header handles interaction with PatchworkOS's file system,
 * following the philosophy that **everything is a file**. This means interacting with physical devices,
 * inter-process communication (like shared memory), and much more is handled via files.
 *
 * ### File Paths and Flags
 *
 * PatchworkOS has a multiroot file system using labels not letters. You may also notice that functions like `open()` do
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
 *
 * ### What really is a file? A small note about Plan9.
 *
 * PatchworkOS uses a broader definition of what a file is then what Plan9 appears to use. Plan9, as far as the author
 * is aware att the time of writing, considers all its system files to be "text files", so all systems are interacted
 * with via strings. For example you can draw a rectangle to the screen using just a text command written to a file. For
 * Patchwork this is very resrictive and results in nonsensical apis (for example consider memory mapped files), thus
 * when it is said that "PatchworkOS follows the everything is a file philosophy", consider that files are defined
 * to be an entry in a hierarchical file system that are interacted with "like a file", a admittedly rather vague
 * definition, but this widens the definition of a file to include files that store binary data instead of just text.
 *
 */

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/**
 * @brief Pipe read end.
 * @ingroup libstd_sys_io
 *
 * The `PIPE_READ` constant defines which file descriptor in `fd` from a `open2` call on the `sys:/pipe/new` file will
 * be the read end of the pipe.
 *
 */
#define PIPE_READ 0
/**
 * @brief Pipe write end.
 * @ingroup libstd_sys_io
 *
 * The `PIPE_WRITE` constant defines which file descriptor in `fd` from a `open2` call on the `sys:/pipe/new` file will
 * be the write end of the pipe.
 *
 */
#define PIPE_WRITE 1

/**
 * @brief System call for opening files.
 * @ingroup libstd_sys_io
 *
 * The `open()` function opens a file located at a given path.
 *
 * @param path The path to the desired file.
 * @return On success, returns the file descriptor, on failure returns `ERR` and errno is set.
 */
fd_t open(const char* path);

/**
 * @brief Wrapper for opening files with a formatted path.
 * @ingroup libstd_sys_io
 *
 * The `openf()` function opens a file located at a path specified by a format string and variable arguments. This is
 * very usefull considering the amount of file path processing required for many of Patchworks APIs.
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
 * The `vopenf()` function is similar to `openf()` but takes a `va_list` for the arguments.
 *
 * @param format The format string specifying the path to the desired file.
 * @param args A `va_list` containing the arguments to be formatted into the path string.
 * @return On success, returns the file descriptor, on failure returns `ERR` and errno is set.
 */
fd_t vopenf(const char* _RESTRICT format, va_list args);

/**
 * @brief System call for opening 2 file descriptors from one file.
 * @ingroup libstd_sys_io

 * The `open2()` function opens a file and returns two file descriptors. This is intended as a more generic
 implementation
 * of system calls like pipe() in for example Linux. One example use case of this system call is as expected pipes, if
 * `open2` is called on `sys:/pipe/new` then `fd[0]` will store the read end of the pipe and `fd[1]` will store the
 write
 * end of the pipe. But if `open()` is called on `sys:/pipe/new` then the returned file descriptor would be both
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

 * The `close()` function closes an open file descriptor.
 *
 * @param fd The file descriptor to close.
 * @return On success, returns 0, on failure returns `ERR` and errno is set.
 */
uint64_t close(fd_t fd);

/**
 * @brief  System call for reading from files.
 * @ingroup libstd_sys_io

 * The `read()` function reads data from a file descriptor into a buffer.
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
 * The `write()` function writes data from a buffer to a file descriptor.
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
 * The `writef()` function writes formatted output to a file descriptor.
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
 * The `vwritef()` function is similar to `writef()` but takes a `va_list` for the arguments.
 *
 * @param fd The file descriptor to write to.
 * @param format The format string.
 * @param args A `va_list` containing the arguments to be formatted.
 * @return On success, returns the number of bytes written. On failure, returns `ERR` and errno is set.
 */
uint64_t vwritef(fd_t fd, const char* _RESTRICT format, va_list args);

/**
 * @brief Type for the `seek` origin argument.
 * @ingroup libstd_sys_io
 *
 * The `seek_origin_t` type is used in the `seek` function to specify the origin.
 *
 */
typedef uint8_t seek_origin_t;

/**
 * @brief System call for changing the file offset.
 * @ingroup libstd_sys_io
 *
 * The `seek()` function moves the offset of the open file associated with the file descriptor `fd`.
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
 * The `chdir()` function changes the current working directory of the calling process.
 *
 * @param path The path to the new directory.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t chdir(const char* path);

/**
 * @brief Poll events type.
 * @ingroup libstd_sys_io
 *
 * The `poll_event_t` enum is used to store events in the `pollfd_t` structure.
 *
 */
typedef enum
{
    POLL_READ = (1 << 0),  //!< File descriptor is/should be ready to read.
    POLL_WRITE = (1 << 1), //!< File descriptor is/should be ready to write.
    POLL_ERR = (1 << 2),   //!< File descriptor caused an error.
    POLL_HUP = (1 << 3),   //!< Stream socket peer closed connection, or shut down writing of connection.
} poll_events_t;

/**
 * @brief Poll file descriptor structure.
 * @ingroup libstd_sys_io
 *
 * The `pollfd_t` type is used in `poll()` to specify which file descriptors to poll and which events to wait for, and
 * to return what events happened.
 *
 */
typedef struct
{
    fd_t fd;               //!< The file descriptor to poll.
    poll_events_t events;  //!< The events to wait for.
    poll_events_t revents; //!< The events that occoured.
} pollfd_t;

/**
 * @brief System call for polling files.
 * @ingroup libstd_sys_io
 *
 * The `poll()` function waits for one of a set of file descriptors to become ready to perform I/O.
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
 * The `poll1()` function waits for events on a single file descriptor. Otherwise it is identical to `poll()` and exists
 * simply for convenience.
 *
 * @param fd The file descriptor to poll.
 * @param events The events to wait for (e.g., `POLL_READ`, `POLL_WRITE`).
 * @param timeout The maximum time (in clock ticks) to wait for an event. If `CLOCKS_NEVER`, it waits forever.
 * @return On success, returns the events that revents. On timeout, returns 0. On failure, the `POLL_ERR` event bit is
 * set and errno is set.
 */
poll_events_t poll1(fd_t fd, poll_events_t events, clock_t timeout);

/**
 * @brief Stat type enum.
 * @ingroup libstd_sys_io
 *
 * The `stat_type_t` enum is used to store file system entry type information in the `stat_t` structure.
 *
 */
typedef enum
{
    STAT_FILE = 0, //!< Is a file.
    STAT_DIR = 1,  //!< Is a directory.
} stat_type_t;

/**
 * @brief Stat type.
 * @ingroup libstd_sys_io
 *
 * The `stat_t` structure is used to to store retrieved information about a entry in the file system.
 *
 */
typedef struct
{
    char name[MAX_NAME]; //!< The name of the entry, not the full filepath.
    stat_type_t type;    //!< The type of the entry (eg,. STAT_FILE, STAT_DIR, etc).
    uint64_t size;       //!< The size of the file on disk in bytes.
} stat_t;

/**
 * @brief System call for retrieving info about a file or directory.
 * @ingroup libstd_sys_io
 *
 * The `stat()` function retrieves information about a file or directory.
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
 * The `ioctl()` function allows drivers to implement unusual behaviour that would be impossible or impractical with a
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
 * The `dup()` function duplicates an open file descriptor. The new file descriptor refers to the same open file
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
 * The `dup2()` function duplicates an open file descriptor to a specified new file descriptor. If `newFd` is already
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
 * The `readdir()` function reads information about every entry in a directory from a directory file descriptor. Think
 * of it like calling `stat()` on everything in the directory. The intention is to call `readdir()` twice once to get
 * the total number of entires in the directory allowing you to allocate a buffer of the correct size, then again to
 * read the entires, ideally this would be done in a loop to make sure the amount of entires does not change between
 * calls to `readdir()`.
 *
 * @param fd The file descriptor of the directory to read.
 * @param infos A pointer to an array of `stat_t` structures where the directory entry information will be stored.
 * @param amount The amount of `stat_t` structures that fit in the infos array.
 * @return On success, returns the total number of entries that exists, NOT the amount of entires read. On failure,
 * returns `ERR` and errno is set.
 */
uint64_t readdir(fd_t fd, stat_t* infos, uint64_t amount);

/**
 * @brief Result type from `allocdir()`.
 * @ingroup libstd_sys_io
 *
 * The `allocdir_t` structure stores the result of the `allocdir()` function.
 *
 */
typedef struct
{
    uint64_t amount; //!< The amount of `stat_t` structures in `allocdir_t::infos`.
    stat_t infos[];  //!< The retrieved `stat_t` structures.
} allocdir_t;

/**
 * @brief Wrapper for easily reading directory entries.
 * @ingroup libstd_sys_io
 *
 * The `allocdir()` function reads all directory entries from a directory file descriptor and allocates memory for them.
 *
 * @param fd The file descriptor of the directory to read.
 * @return On success, returns a pointer to an `allocdir_t` structure containing the number of entries and an array of
 * `stat_t` structures, when you are done with the structure make sure to free it using `free()`. On failure, returns
 * `NULL` and errno is set.
 */
allocdir_t* allocdir(fd_t fd);

/**
 * @brief Wrapper for creating a directory.
 * @ingroup libstd_sys_io
 *
 * The `mkdir()` function creates a new directory.
 *
 * @param path The path of the directory to create.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t mkdir(const char* path);

#if defined(__cplusplus)
}
#endif

#endif
