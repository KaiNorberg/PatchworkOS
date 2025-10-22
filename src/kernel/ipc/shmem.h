#pragma once

#include "fs/sysfs.h"
#include "sync/lock.h"
#include "utils/ref.h"

#include <sys/io.h>
#include <sys/list.h>

/**
 * @brief Shared Memory
 * @ingroup kernel_ipc
 * @defgroup kernel_ipc_shmem Shared Memory
 *
 * Shared memory is exposed in the `/dev/shmem` directory. Shared memory allows multiple processes to share a section of
 * memory for inter-process communication (IPC).
 *
 * ## Creating Shared Memory
 *
 * Shared memory objects are created using the `/dev/shmem/new` file. Opening this file using `open()` will create a new
 * shared memory object and return a file descriptor to it.
 *
 * ## Using Shared Memory
 *
 * Shared memory objects can be mapped to the current process's address space using the `mmap()` system call. The first
 * call to `mmap()` will decide the size of the shared memory object. Subsequent calls to `mmap()` will map the existing
 * shared memory object.
 *
 * @{
 */

/**
 * @brief Represents a shared memory object.
 */
typedef struct
{
    ref_t ref;
    uint64_t pageAmount;
    void** pages;
    lock_t lock;
} shmem_object_t;

/**
 * @brief Initializes the shared memory subsystem.
 */
void shmem_init(void);

/** @} */
