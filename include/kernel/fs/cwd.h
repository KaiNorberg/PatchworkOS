#pragma once

#include <kernel/fs/namespace.h>
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
 * Will by default lazily resolve to the root path of the namespace until set to another path.
 *
 * @param cwd The CWD structure to initialize.
 */
void cwd_init(cwd_t* cwd);

/**
 * @brief Deinitialize a CWD structure.
 *
 * @param cwd The CWD structure to deinitialize.
 */
void cwd_deinit(cwd_t* cwd);

/**
 * @brief Get the current working directory.
 *
 * @note If the cwd has not been set, this will return the root path of the namespace. This is to solve
 * a circular dependency where the kernel process needs to be initialized before the vfs.
 *
 * @param cwd The CWD structure.
 * @param ns The namespace to get the root path from if the cwd is not set.
 * @return The current working directory path.
 */
path_t cwd_get(cwd_t* cwd, namespace_t* ns);

/**
 * @brief Set the current working directory.
 *
 * @param cwd The CWD structure.
 * @param newPath The new current working directory.
 */
void cwd_set(cwd_t* cwd, const path_t* newPath);

/**
 * @brief Clear the current working directory.
 *
 * Needed as a process might have its working directory inside its own `/proc/[pid]` directory which, since that
 * directory holds references to the process itself, would result in a memory leak.
 *
 * @param cwd The CWD structure.
 */
void cwd_clear(cwd_t* cwd);

/** @} */