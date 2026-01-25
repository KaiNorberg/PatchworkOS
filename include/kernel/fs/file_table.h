#pragma once

#include <kernel/config.h>
#include <kernel/fs/file.h>
#include <kernel/sync/lock.h>

#include <sys/bitmap.h>

/**
 * @brief File Table
 * @defgroup kernel_fs_file_table File Table
 * @ingroup kernel_fs
 *
 * The file table is a per-process structure that keeps track of all open files for a process.
 *
 * @{
 */

/**
 * @brief File table structure.
 * @struct file_table_t
 */
typedef struct file_table
{
    file_t* files[CONFIG_MAX_FD];
    BITMAP_DEFINE(bitmap, CONFIG_MAX_FD);
    lock_t lock;
} file_table_t;

/**
 * @brief Initialize a file table.
 *
 * @param table The file table to initialize.
 */
void file_table_init(file_table_t* table);

/**
 * @brief Deinitialize a file table.
 *
 * This will close all open files in the table.
 *
 * @param table The file table to deinitialize.
 */
void file_table_deinit(file_table_t* table);

/**
 * @brief Get a file from its file descriptor.
 *
 * @param table The file table.
 * @param fd The file descriptor.
 * @return On success, a new reference to the file. On failure, returns `NULL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBADF`: The file descriptor is invalid.
 */
file_t* file_table_get(file_table_t* table, fd_t fd);

/**
 * @brief Allocate a new file descriptor for a file.
 *
 * @param table The file table.
 * @param file The file to associate with the new file descriptor.
 * @return On success, the allocated file descriptor. On failure, `_FAIL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EMFILE`: Too many open files.
 */
fd_t file_table_open(file_table_t* table, file_t* file);

/**
 * @brief Free a file descriptor.
 *
 * If the file has no other references, it will be closed.
 *
 * @param table The file table.
 * @param fd The file descriptor to free.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBADF`: The file descriptor is invalid.
 */
uint64_t file_table_close(file_table_t* table, fd_t fd);

/**
 * @brief Close all files in the file table.
 *
 * @param table The file table.
 */
void file_table_close_all(file_table_t* table);

/**
 * @brief Close all files in the file table with the specified mode.
 *
 * @param table The file table.
 * @param mode The mode to close files with.
 */
void file_table_close_mode(file_table_t* table, mode_t mode);

/**
 * @brief Free a range of file descriptors.
 *
 * If the files have no other references, they will be closed.
 *
 * @param table The file table.
 * @param min The minimum file descriptor to free, inclusive.
 * @param max The maximum file descriptor to free, exclusive.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 */
uint64_t file_table_close_range(file_table_t* table, fd_t min, fd_t max);

/**
 * @brief Set a specific file descriptor to a file.
 *
 * If the file descriptor is already in use, the old file will be closed.
 *
 * @param table The file table.
 * @param fd The file descriptor to set.
 * @param file The file to associate with the file descriptor.
 * @return On success, `fd`. On failure, `_FAIL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBADF`: The file descriptor is invalid.
 */
fd_t file_table_set(file_table_t* table, fd_t fd, file_t* file);

/**
 * @brief Duplicate a file descriptor.
 *
 * Allocates a new file descriptor that refers to the same file as `oldFd`.
 *
 * @param table The file table.
 * @param oldFd The file descriptor to duplicate.
 * @return On success, the new file descriptor. On failure, `_FAIL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBADF`: The file descriptor is invalid.
 * - `EMFILE`: Too many open files.
 */
fd_t file_table_dup(file_table_t* table, fd_t oldFd);

/**
 * @brief Duplicate a file descriptor to a specific file descriptor.
 *
 * If `newFd` is already in use, the old file will be closed.
 *
 * @param table The file table.
 * @param oldFd The file descriptor to duplicate.
 * @param newFd The file descriptor to duplicate to.
 * @return On success, `newFd`. On failure, `_FAIL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 * - `EBADF`: One of the file descriptors is invalid.
 * - `EMFILE`: Too many open files.
 */
fd_t file_table_dup2(file_table_t* table, fd_t oldFd, fd_t newFd);

/**
 * @brief Copy a file table, closing any overlapping file descriptors.
 *
 * @param dest The destination file table.
 * @param src The source file table.
 * @param min The minimum file descriptor to copy, inclusive.
 * @param max The maximum file descriptor to copy, exclusive.
 * @return On success, the number of copied file descriptors. On failure, `_FAIL` and `errno` is set to:
 * - `EINVAL`: Invalid parameters.
 */
uint64_t file_table_copy(file_table_t* dest, file_table_t* src, fd_t min, fd_t max);

/** @} */