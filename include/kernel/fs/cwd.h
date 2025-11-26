#pragma once

#include <kernel/fs/path.h>
#include <kernel/sync/lock.h>

/**
 * @brief Current Working Directory
 * @defgroup kernel_fs_cwd Current Working Directory
 * @ingroup kernel_fs
 *
 * The current working directory (CWD) is a per-process structure to track the current location in the filesystem for
 * the process.
 *
 * @{
 */

typedef struct cwd
{
    path_t path;
    lock_t lock;
} cwd_t;

/**
 * @brief Initialize a CWD structure.
 *
 * @param cwd The CWD structure to initialize.
 * @param initialPath The initial path to set as the current working directory, can be `NULL` for root.
 */
void cwd_init(cwd_t* cwd, const path_t* initialPath);

/**
 * @brief Deinitialize a CWD structure.
 *
 * @param cwd The CWD structure to deinitialize.
 */
void cwd_deinit(cwd_t* cwd);

/**
 * @brief Get the current working directory.
 *
 * @note If the `cwd_init()` was called with `initialPath` as `NULL` and the CWD has not been set, this will return the
 * root path of the kernel process's namespace. This is to solve a circular dependency where the kernel process needs to
 * be initialized before the vfs.
 *
 * @param cwd The CWD structure.
 * @return The current working directory path.
 */
path_t cwd_get(cwd_t* cwd);

/**
 * @brief Set the current working directory.
 *
 * @param cwd The CWD structure.
 * @param newPath The new current working directory.
 */
void cwd_set(cwd_t* cwd, const path_t* newPath);

/** @} */