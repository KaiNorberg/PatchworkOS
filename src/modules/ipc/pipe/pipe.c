#include <kernel/fs/file.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/module/module.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/ring.h>

#include <assert.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>

/**
 * @brief Pipes.
 * @defgroup modules_ipc_pipe Pipes
 * @ingroup modules_ipc
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

static dentry_t* pipeDir = NULL;
static dentry_t* newFile = NULL;

static uint64_t pipe_open(file_t* file)
{
    pipe_private_t* private = malloc(sizeof(pipe_private_t));
    if (private == NULL)
    {
        return ERR;
    }
    private->buffer = pmm_alloc();
    if (private->buffer == NULL)
    {
        free(private);
        return ERR;
    }
    ring_init(&private->ring, private->buffer, PAGE_SIZE);
    private->isReadClosed = false;
    private->isWriteClosed = false;
    wait_queue_init(&private->waitQueue);
    lock_init(&private->lock);

    private->readEnd = file;
    private->writeEnd = file;

    file->private = private;
    return 0;
}

static uint64_t pipe_open2(file_t* files[2])
{
    pipe_private_t* private = malloc(sizeof(pipe_private_t));
    if (private == NULL)
    {
        return ERR;
    }
    private->buffer = pmm_alloc();
    if (private->buffer == NULL)
    {
        free(private);
        return ERR;
    }
    ring_init(&private->ring, private->buffer, PAGE_SIZE);
    private->isReadClosed = false;
    private->isWriteClosed = false;
    wait_queue_init(&private->waitQueue);
    lock_init(&private->lock);

    private->readEnd = files[PIPE_READ];
    private->writeEnd = files[PIPE_WRITE];

    files[0]->private = private;
    files[1]->private = private;
    return 0;
}

static void pipe_close(file_t* file)
{
    pipe_private_t* private = file->private;
    lock_acquire(&private->lock);
    if (private->readEnd == file)
    {
        private->isReadClosed = true;
    }
    if (private->writeEnd == file)
    {
        private->isWriteClosed = true;
    }

    wait_unblock(&private->waitQueue, WAIT_ALL, EOK);
    if (private->isWriteClosed && private->isReadClosed)
    {
        lock_release(&private->lock);
        wait_queue_deinit(&private->waitQueue);
        pmm_free(private->buffer);
        free(private);
        return;
    }

    lock_release(&private->lock);
}

static uint64_t pipe_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    if (count == 0)
    {
        return 0;
    }

    pipe_private_t* private = file->private;
    if (private->readEnd != file)
    {
        errno = ENOTSUP;
        return ERR;
    }

    if (count >= PAGE_SIZE)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&private->lock);

    if (WAIT_BLOCK_LOCK(&private->waitQueue, &private->lock,
            ring_data_length(&private->ring) != 0 || private->isWriteClosed) == ERR)
    {
        return ERR;
    }

    count = MIN(count, ring_data_length(&private->ring));
    if (ring_read(&private->ring, buffer, count) == ERR)
    {
        panic(NULL, "Failed to read from pipe");
    }

    wait_unblock(&private->waitQueue, WAIT_ALL, EOK);

    *offset += count;
    return count;
}

static uint64_t pipe_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    pipe_private_t* private = file->private;
    if (private->writeEnd != file)
    {
        errno = ENOTSUP;
        return ERR;
    }

    if (count >= PAGE_SIZE)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&private->lock);

    if (WAIT_BLOCK_LOCK(&private->waitQueue, &private->lock,
            ring_free_length(&private->ring) >= count || private->isReadClosed) == ERR)
    {
        return ERR;
    }

    if (private->isReadClosed)
    {
        wait_unblock(&private->waitQueue, WAIT_ALL, EOK);
        errno = EPIPE;
        return ERR;
    }

    if (ring_write(&private->ring, buffer, count) == ERR)
    {
        panic(NULL, "Failed to write to pipe");
    }

    wait_unblock(&private->waitQueue, WAIT_ALL, EOK);

    *offset += count;
    return count;
}

static wait_queue_t* pipe_poll(file_t* file, poll_events_t* revents)
{
    pipe_private_t* private = file->private;
    LOCK_SCOPE(&private->lock);

    if (ring_data_length(&private->ring) != 0 || private->isWriteClosed)
    {
        *revents |= POLLIN;
    }
    if (ring_free_length(&private->ring) != 0 || private->isReadClosed)
    {
        *revents |= POLLOUT;
    }

    return &private->waitQueue;
}

static file_ops_t fileOps = {
    .open = pipe_open,
    .open2 = pipe_open2,
    .close = pipe_close,
    .read = pipe_read,
    .write = pipe_write,
    .poll = pipe_poll,
};

uint64_t pipe_init(void)
{
    pipeDir = sysfs_dir_new(NULL, "pipe", NULL, NULL);
    if (pipeDir == NULL)
    {
        LOG_ERR("failed to initialize pipe directory");
        return ERR;
    }

    newFile = sysfs_file_new(pipeDir, "new", NULL, &fileOps, NULL);
    if (newFile == NULL)
    {
        UNREF(pipeDir);
        LOG_ERR("failed to initialize pipe new file");
        return ERR;
    }

    return 0;
}

void pipe_deinit(void)
{
    UNREF(newFile);
    newFile = NULL;
    UNREF(pipeDir);
    pipeDir = NULL;
}

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        if (pipe_init() == ERR)
        {
            return ERR;
        }
        break;
    case MODULE_EVENT_UNLOAD:
        pipe_deinit();
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("Pipes", "Kai Norberg", "Implements pipes for inter-process communication", OS_VERSION, "MIT",
    "BOOT_ALWAYS");