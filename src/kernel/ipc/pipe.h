#pragma once

#include "fs/vfs.h"
#include "sched/sched.h"
#include "sched/wait.h"
#include "utils/ring.h"

/**
 * @brief Pipes.
 * @defgroup kernel_ipc_pipe Pipes
 * @ingroup kernel_ipc
 *
 * @{
 */

typedef struct
{
    void* buffer;
    ring_t ring;
    bool isReadClosed;
    bool isWriteClosed;
    wait_queue_t waitQueue;
    lock_t lock;
    // Note: These pointers are just for checking which end the current file is, they should not be referenced.
    void* readEnd;
    void* writeEnd;
} pipe_private_t;

void pipe_init(void);

/** @} */
