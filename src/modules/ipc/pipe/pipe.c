#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
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
} pipe_t;

static dentry_t* pipeDir = NULL;
static dentry_t* newFile = NULL;

static uint64_t pipe_open(file_t* file)
{
    pipe_t* data = malloc(sizeof(pipe_t));
    if (data == NULL)
    {
        return ERR;
    }
    data->buffer = pmm_alloc();
    if (data->buffer == NULL)
    {
        free(data);
        return ERR;
    }
    ring_init(&data->ring, data->buffer, PAGE_SIZE);
    data->isReadClosed = false;
    data->isWriteClosed = false;
    wait_queue_init(&data->waitQueue);
    lock_init(&data->lock);
    data->readEnd = file;
    data->writeEnd = file;

    file->private = data;
    return 0;
}

static uint64_t pipe_open2(file_t* files[2])
{
    pipe_t* data = malloc(sizeof(pipe_t));
    if (data == NULL)
    {
        return ERR;
    }
    data->buffer = pmm_alloc();
    if (data->buffer == NULL)
    {
        free(data);
        return ERR;
    }
    ring_init(&data->ring, data->buffer, PAGE_SIZE);
    data->isReadClosed = false;
    data->isWriteClosed = false;
    wait_queue_init(&data->waitQueue);
    lock_init(&data->lock);

    data->readEnd = files[PIPE_READ];
    data->writeEnd = files[PIPE_WRITE];

    files[0]->private = data;
    files[1]->private = data;
    return 0;
}

static void pipe_close(file_t* file)
{
    pipe_t* data = file->private;
    lock_acquire(&data->lock);
    if (data->readEnd == file)
    {
        data->isReadClosed = true;
    }
    if (data->writeEnd == file)
    {
        data->isWriteClosed = true;
    }

    wait_unblock(&data->waitQueue, WAIT_ALL, EOK);
    if (data->isWriteClosed && data->isReadClosed)
    {
        lock_release(&data->lock);
        wait_queue_deinit(&data->waitQueue);
        pmm_free(data->buffer);
        free(data);
        return;
    }

    lock_release(&data->lock);
}

static uint64_t pipe_read(file_t* file, void* buffer, uint64_t count, uint64_t* offset)
{
    UNUSED(offset);

    if (count == 0)
    {
        return 0;
    }

    pipe_t* data = file->private;
    if (data->readEnd != file)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (count >= PAGE_SIZE)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&data->lock);

    if (ring_bytes_used(&data->ring, NULL) == 0)
    {
        if (file->mode & MODE_NONBLOCK)
        {
            errno = EAGAIN;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&data->waitQueue, &data->lock,
                ring_bytes_used(&data->ring, NULL) != 0 || data->isWriteClosed) == ERR)
        {
            return ERR;
        }
    }

    uint64_t result = ring_read(&data->ring, buffer, count, NULL);
    wait_unblock(&data->waitQueue, WAIT_ALL, EOK);
    return result;
}

static uint64_t pipe_write(file_t* file, const void* buffer, uint64_t count, uint64_t* offset)
{
    UNUSED(offset);

    pipe_t* data = file->private;
    if (data->writeEnd != file)
    {
        errno = ENOSYS;
        return ERR;
    }

    if (count >= PAGE_SIZE)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&data->lock);

    if (ring_bytes_free(&data->ring, NULL) == 0)
    {
        if (file->mode & MODE_NONBLOCK)
        {
            errno = EAGAIN;
            return ERR;
        }

        if (WAIT_BLOCK_LOCK(&data->waitQueue, &data->lock,
                ring_bytes_free(&data->ring, NULL) != 0 || data->isReadClosed) == ERR)
        {
            return ERR;
        }
    }

    if (data->isReadClosed)
    {
        wait_unblock(&data->waitQueue, WAIT_ALL, EOK);
        errno = EPIPE;
        return ERR;
    }

    uint64_t result = ring_write(&data->ring, buffer, count, NULL);
    wait_unblock(&data->waitQueue, WAIT_ALL, EOK);
    return result;
}

static wait_queue_t* pipe_poll(file_t* file, poll_events_t* revents)
{
    pipe_t* data = file->private;
    LOCK_SCOPE(&data->lock);

    if (ring_bytes_used(&data->ring, NULL) != 0 || data->isWriteClosed)
    {
        *revents |= POLLIN;
    }
    if (ring_bytes_free(&data->ring, NULL) > 0 || data->isReadClosed)
    {
        *revents |= POLLOUT;
    }
    if ((file == data->readEnd && data->isWriteClosed) || (file == data->writeEnd && data->isReadClosed))
    {
        *revents |= POLLHUP;
    }

    return &data->waitQueue;
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
    pipeDir = devfs_dir_new(NULL, "pipe", NULL, NULL);
    if (pipeDir == NULL)
    {
        LOG_ERR("failed to initialize pipe directory");
        return ERR;
    }

    newFile = devfs_file_new(pipeDir, "new", NULL, &fileOps, NULL);
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