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
 * Patchwork implements shared memory via the `/dev/shmem` folder.
 *
 * A new shared memory object can be created by opening the `/dev/shmem/new` file, the opened file will contain the new
 * shared memory object. The new shared memory object can also be accessed via the `/dev/shmem/[id]` file, where the id
 * can be retrieved by reading from the shared memory file.
 *
 * By default, only the owner of the shared memory object and its children can access the shared memory object, but by
 * writing the `grant [pid]` and `revoke [pid]` commands to the shared memory file its possible to give access to
 * additional processes.
 *
 * The actual shared memory segment is created on the first call to `mmap()` where the section will then be a fixed size
 * equal to the size specified on the first call to `mmap()`, subsequent calls to `mmap()` will map the same section.
 * Note that even if the file is closed the mapped memory sections will continue to be valid.
 *
 * @{
 */

/**
 * @brief Keeps track of a process that is allowed to access a shared memory object.
 */
typedef struct
{
    list_entry_t entry;
    pid_t pid;
} shmem_allowed_process_t;

/**
 * @brief Represents a shared memory object.
 */
typedef struct
{
    ref_t ref;
    char id[MAX_NAME];
    sysfs_file_t file;
    list_t allowedProcesses;
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
