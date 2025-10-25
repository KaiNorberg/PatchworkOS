#pragma once

#include <sys/io.h>

#include "config.h"
#include "fs/path.h"
#include "log/log.h"
#include "sync/lock.h"
#include "utils/bitmap.h"

typedef struct file file_t;
typedef struct dir_entry dir_entry_t;

/**
 * @brief Virtual File System context.
 * @defgroup kernel_fs_vfs_ctx VFS Context
 * @ingroup kernel_fs
 *
 * A VFS context represents the state of the VFS for a single process. It contains the current working directory and
 * the open file descriptors.
 *
 * @{
 */

/**
 * @brief VFS context structure.
 * @struct vfs_ctx_t
 *
 * TODO: Implement bitmap based fd tracking.
 */
typedef struct
{
    path_t cwd;
    file_t* files[CONFIG_MAX_FD];
    lock_t lock;
    bool initalized;
} vfs_ctx_t;

/**
 * @brief Initialize a VFS context.
 *
 * @param ctx The context to initialize.
 * @param cwd The initial current working directory, can be `NULL`.
 */
void vfs_ctx_init(vfs_ctx_t* ctx, const path_t* cwd);

/**
 * @brief Deinitialize a VFS context.
 *
 * This will close all open file descriptors and release the current working directory.
 *
 * @param ctx The context to deinitialize.
 */
void vfs_ctx_deinit(vfs_ctx_t* ctx);

/**
 * @brief Get a file from a VFS context.
 *
 * @param ctx The context to get the file from.
 * @param fd The file descriptor to get.
 * @return On success, a new reference to the file. On failure, returns `NULL` and sets `errno`.
 */
file_t* vfs_ctx_get_file(vfs_ctx_t* ctx, fd_t fd);

/**
 * @Get the current working directory of a VFS context.
 *
 * If the current directory is `NULL`, then returns a reference to the root of the kernel process's namespace.
 *
 * Will create a new reference to the cwd.
 *
 * @param ctx The context to get the current working directory of.
 * @param outCwd The output path to store the current working directory in.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t vfs_ctx_get_cwd(vfs_ctx_t* ctx, path_t* outCwd);

/**
 * @brief Set the current working directory of a VFS context.
 *
 * Will create a new reference to the cwd and release the old one.
 *
 * @param ctx The context to set the current working directory of.
 * @param cwd The new current working directory.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t vfs_ctx_set_cwd(vfs_ctx_t* ctx, const path_t* cwd);

/**
 * @brief Allocate a new file descriptor in a VFS context.
 *
 * Will create a new reference to the file.
 *
 * @param ctx The context to open the file descriptor in.
 * @param file The file to open.
 * @return On success, the new file descriptor. On failure, returns `ERR` and sets `errno`.
 */
fd_t vfs_ctx_alloc_fd(vfs_ctx_t* ctx, file_t* file);

/**
 * @brief Allocate a specific file descriptor in a VFS context.
 *
 * Will create a new reference to the file and release the old one if it exists.
 *
 * @param ctx The context to open the file descriptor in.
 * @param fd The file descriptor to set.
 * @param file The file to open.
 * @return On success, the file descriptor. On failure, returns `ERR` and sets `errno`.
 */
fd_t vfs_ctx_set_fd(vfs_ctx_t* ctx, fd_t fd, file_t* file);

/**
 * @brief Free a file descriptor in a VFS context.
 *
 * Will release the reference to the file.
 *
 * @param ctx The context to close the file descriptor in.
 * @param fd The file descriptor to close.
 * @return On success, `0`. On failure, returns `ERR` and sets `errno`.
 */
uint64_t vfs_ctx_free_fd(vfs_ctx_t* ctx, fd_t fd);

/**
 * @brief Duplicate a file descriptor in a VFS context.
 *
 * Will create a new reference to the file.
 *
 * @param ctx The context to duplicate the file descriptor in.
 * @param oldFd The file descriptor to duplicate.
 * @return On success, the new file descriptor. On failure, returns `ERR` and sets `errno`.
 */
fd_t vfs_ctx_dup(vfs_ctx_t* ctx, fd_t oldFd);

/**
 * @brief Duplicate a file descriptor in a VFS context to a specific file descriptor.
 *
 * Will create a new reference to the file and if the newFd is already in use, it will be closed first.
 *
 * @param ctx The context to duplicate the file descriptor in.
 * @param oldFd The file descriptor to duplicate.
 * @param newFd The file descriptor to duplicate to.
 * @return On success, the new file descriptor. On failure, returns `ERR` and sets `errno`.
 */
fd_t vfs_ctx_dup2(vfs_ctx_t* ctx, fd_t oldFd, fd_t newFd);

/** @} */
