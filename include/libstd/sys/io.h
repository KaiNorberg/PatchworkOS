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
#include "_internal/time_t.h"

/**
 * @brief System IO header.
 * @ingroup libstd
 * @defgroup libstd_sys_io System IO
 *
 * The `sys/io.h` header handles interaction with PatchworkOS's file system,
 * following the philosophy that **everything is a file**. This means interacting with physical devices,
 * inter-process communication (like shared memory), and much more is handled via files.
 *
 * ### Flags
 *
 * Functions like `open()` do not have a specific argument for flags, instead the filepath itself contains the flags.
 * This means that for example there is no need for a special "truncate" redirect in a shell (>>) instead you can just
 * add the "trunc" flag to the filepath and use a normal redirect (>).
 *
 * Here is an example filepath: `/this/is/a/path?with&some&flags`.
 *
 * Check the 'src/kernel/fs/path.h' file for a list of available flags.
 *
 */

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/**
 * @brief Pipe read end.
 * @ingroup libstd_sys_io
 *
 * The `PIPE_READ` constant defines which file descriptor in `fd` from a `open2` call on the `/dev/pipe/new` file will
 * be the read end of the pipe.
 *
 */
#define PIPE_READ 0
/**
 * @brief Pipe write end.
 * @ingroup libstd_sys_io
 *
 * The `PIPE_WRITE` constant defines which file descriptor in `fd` from a `open2` call on the `/dev/pipe/new` file will
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
 * @brief Wrapper for opening files with a formatted path, using a `va_list`.
 * @ingroup libstd_sys_io
 *
 * @param format The format string specifying the path to the desired file.
 * @param args A `va_list` containing the arguments to be formatted into the path string.
 * @return On success, returns the file descriptor, on failure returns `ERR` and errno is set.
 */
fd_t vopenf(const char* _RESTRICT format, va_list args);

/**
 * @brief System call for opening 2 file descriptors from one file.
 * @ingroup libstd_sys_io

 * This is intended as a more generic
 implementation
 * of system calls like pipe() in for example Linux. One example use case of this system call is pipes, if
 * `open2` is called on `/dev/pipe/new` then `fd[0]` will store the read end of the pipe and `fd[1]` will store the
 write
 * end of the pipe. But if `open()` is called on `/dev/pipe/new` then the returned file descriptor would be both
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
 *
 * @param fd The file descriptor to close.
 * @return On success, returns 0, on failure returns `ERR` and errno is set.
 */
uint64_t close(fd_t fd);

/**
 * @brief  System call for reading from files.
 * @ingroup libstd_sys_io
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
 * @param fd The file descriptor to write to.
 * @param format The format string.
 * @param ... Variable arguments to be formatted.
 * @return On success, returns the number of bytes written. On failure, returns `ERR` and errno is set.
 */
uint64_t writef(fd_t fd, const char* _RESTRICT format, ...);

/**
 * @brief Wrapper for writing a formatted string to a file with a `va_list`.
 * @ingroup libstd_sys_io
 *
 * @param fd The file descriptor to write to.
 * @param format The format string.
 * @param args A `va_list` containing the arguments to be formatted.
 * @return On success, returns the number of bytes written. On failure, returns `ERR` and errno is set.
 */
uint64_t vwritef(fd_t fd, const char* _RESTRICT format, va_list args);

/**
 * @brief Type for the `seek()` origin argument.
 * @ingroup libstd_sys_io
 *
 */
typedef uint8_t seek_origin_t;

/**
 * @brief System call for changing the file offset.
 * @ingroup libstd_sys_io
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
 * @param path The path to the new directory.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t chdir(const char* path);

/**
 * @brief Poll events type.
 * @ingroup libstd_sys_io
 *
 */
typedef enum
{
    POLLNONE = 0,        //!< None
    POLLIN = (1 << 0),   //!< File descriptor is ready to read.
    POLLOUT = (1 << 1),  //!< File descriptor is ready to write.
    POLLERR = (1 << 2),  //!< File descriptor caused an error.
    POLLHUP = (1 << 3),  //!< Stream socket peer closed connection, or shut down writing of connection.
    POLLNVAL = (1 << 4), //!< Invalid file descriptor.
} poll_events_t;

/**
 * @brief Poll event values that will always be checked and included even if not specified.
 * @ingroup libstd_sys_io
 */
#define POLL_SPECIAL (POLLERR | POLLHUP | POLLNVAL)

/**
 * @brief Poll file descriptor structure.
 * @ingroup libstd_sys_io
 *
 */
typedef struct pollfd
{
    fd_t fd;               //!< The file descriptor to poll.
    poll_events_t events;  //!< The events to wait for.
    poll_events_t revents; //!< The events that occurred.
} pollfd_t;

/**
 * @brief System call for polling files.
 * @ingroup libstd_sys_io
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
 * @param events The events to wait for (e.g., `POLLIN`, `POLLOUT`).
 * @param timeout The maximum time (in clock ticks) to wait for an event. If `CLOCKS_NEVER`, it waits forever.
 * @return On success, returns the events that occurred. On timeout, returns 0. On failure, the `POLLERR` event bit is
 * set and errno is set.
 */
poll_events_t poll1(fd_t fd, poll_events_t events, clock_t timeout);

/**
 * @brief Inode type enum.
 * @ingroup libstd_sys_io
 *
 */
typedef enum
{
    INODE_FILE, //!< Is a file.
    INODE_DIR,  //!< Is a directory.
} inode_type_t;

/**
 * @brief Inode number enum.
 * @ingroup libstd_sys_io
 *
 */
typedef uint64_t inode_number_t;

/**
 * @brief Stat type.
 * @ingroup libstd_sys_io
 *
 */
typedef struct
{
    inode_number_t number; //!< The number of the entries inode.
    inode_type_t type;     //!< The type of the entries inode.
    uint64_t size;         //!< The size of the file that is visible outside the filesystem.
    uint64_t blocks;       //!< The amount of blocks used on disk to store the file.
    uint64_t linkAmount;   //!< The amount of times the inode appears in dentries.
    time_t accessTime;     //!< Unix time stamp for the last inode access.
    time_t modifyTime;     //!< Unix time stamp for last file content alteration.
    time_t changeTime;     //!< Unix time stamp for the last file metadata alteration.
    char name[MAX_NAME];   //!< The name of the entry, not the full filepath.
    uint8_t padding[64];   //!< Padding to leave space for future expansion.
} stat_t;

#ifdef static_assert
static_assert(sizeof(stat_t) == 160, "invalid stat_t size");
#endif

/**
 * @brief System call for retrieving info about a file or directory.
 * @ingroup libstd_sys_io
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
 * @param oldFd The open file descriptor to duplicate.
 * @return On success, returns the new file descriptor. On failure, returns `ERR` and errno is set.
 */
fd_t dup(fd_t oldFd);

/**
 * @brief System call for duplicating file descriptors, with a destination.
 * @ingroup libstd_sys_io
 *
 * @param oldFd The open file descriptor to duplicate.
 * @param newFd The desired new file descriptor.
 * @return On success, returns the new file descriptor. On failure, returns `ERR` and errno is set.
 */
fd_t dup2(fd_t oldFd, fd_t newFd);

/**
 * @brief Directory entry struct.
 * @ingroup libstd_sys_io
 *
 */
typedef struct
{
    inode_number_t number; //!< The number of the entries inode.
    inode_type_t type;     //!< The type of the entries inode.
    char name[MAX_NAME];   //!< The name of the entry, not the full file path.
} dirent_t;

/**
 * @brief System call for reading directory entires.
 * @ingroup libstd_sys_io
 *
 * @param fd The file descriptor of the directory to read.
 * @param buffer The destination buffer.
 * @param count The size of the buffer in bytes.
 * @return On success, returns the total number of bytes written to the buffer. On failure,
 * returns `ERR` and errno is set.
 */
uint64_t getdents(fd_t fd, dirent_t* buffer, uint64_t count);

/**
 * @brief Wrapper for creating a directory.
 * @ingroup libstd_sys_io
 *
 * @param path The path of the directory to create.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t mkdir(const char* path);

/**
 * @brief System call for removing a file.
 * @ingroup libstd_sys_io
 *
 * @param path The path of the file to remove.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t unlink(const char* path);

/**
 * @brief System call for removing a directory.
 * @ingroup libstd_sys_io
 *
 * @param path The path of the directory to remove.
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t rmdir(const char* path);

/**
 * @brief System call for creating a hardlink.
 *
 * @param oldPath
 * @param newPath
 * @return On success, returns 0. On failure, returns `ERR` and errno is set.
 */
uint64_t link(const char* oldPath, const char* newPath);

#if defined(__cplusplus)
}
#endif

#endif
