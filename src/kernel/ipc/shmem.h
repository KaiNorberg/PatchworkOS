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
 *
 * TODO: Add namespace support and namespace sharing.
 *
 * @{
 */

/**
 * @brief Represents a shared memory object.
 */
typedef struct
{
    ref_t ref;
    char id[MAX_NAME];
    sysfs_file_t file;
    pid_t owner;
    uint64_t pageAmount;
    void** pages;
    lock_t lock;
} shmem_object_t;

/**
 * @brief Initializes the shared memory subsystem.
 */
void shmem_init(void);

/** @} */
