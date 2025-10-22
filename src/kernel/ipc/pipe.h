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
 * Pipes are exposed in the `/dev/pipe` directory. Pipes are unidirectional communication channels that can be used for
 * inter-process communication (IPC).
 *
 * ## Creating Pipes
 *
 * Pipes are created using the `/dev/pipe/new` file. Opening this file using `open()` will return one file descriptor
 * that can be used for both reading and writing. To create a pipe with separate file descriptors for reading and
 * writing, use `open2()` with the `/dev/pipe/new` file.
 *
 * ## Using Pipes
 *
 * Pipes can be read from and written to using the expected `read()` and `write()` system calls. Pipes are blocking and
 * pollable, following expected POSIX semantics.
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
