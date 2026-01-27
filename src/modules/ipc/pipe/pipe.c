#include <kernel/fs/file.h>
#include <kernel/fs/path.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/pmm.h>
#include <kernel/module/module.h>
#include <kernel/sched/thread.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/fifo.h>

#include <assert.h>
#include <stdlib.h>
#include <sys/fs.h>
#include <sys/math.h>

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
    fifo_t ring;
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

static status_t pipe_open(file_t* file)
{
    pipe_t* data = malloc(sizeof(pipe_t));
    if (data == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }
    data->buffer = malloc(PAGE_SIZE);
    if (data->buffer == NULL)
    {
        free(data);
        return ERR(DRIVER, NOMEM);
    }
    fifo_init(&data->ring, data->buffer, PAGE_SIZE);
    data->isReadClosed = false;
    data->isWriteClosed = false;
    wait_queue_init(&data->waitQueue);
    lock_init(&data->lock);
    data->readEnd = file;
    data->writeEnd = file;

    file->data = data;
    return OK;
}

static status_t pipe_open2(file_t* files[2])
{
    pipe_t* data = malloc(sizeof(pipe_t));
    if (data == NULL)
    {
        return ERR(DRIVER, NOMEM);
    }
    data->buffer = malloc(PAGE_SIZE);
    if (data->buffer == NULL)
    {
        free(data);
        return ERR(DRIVER, NOMEM);
    }
    fifo_init(&data->ring, data->buffer, PAGE_SIZE);
    data->isReadClosed = false;
    data->isWriteClosed = false;
    wait_queue_init(&data->waitQueue);
    lock_init(&data->lock);

    data->readEnd = files[PIPE_READ];
    data->writeEnd = files[PIPE_WRITE];

    files[0]->data = data;
    files[1]->data = data;
    return OK;
}

static void pipe_close(file_t* file)
{
    pipe_t* data = file->data;
    lock_acquire(&data->lock);
    if (data->readEnd == file)
    {
        data->isReadClosed = true;
    }
    if (data->writeEnd == file)
    {
        data->isWriteClosed = true;
    }

    wait_unblock(&data->waitQueue, WAIT_ALL, OK);
    if (data->isWriteClosed && data->isReadClosed)
    {
        lock_release(&data->lock);
        wait_queue_deinit(&data->waitQueue);
        free(data->buffer);
        free(data);
        return;
    }

    lock_release(&data->lock);
}

static status_t pipe_read(file_t* file, void* buffer, size_t count, size_t* offset, size_t* bytesRead)
{
    UNUSED(offset);

    if (count == 0)
    {
        *bytesRead = 0;
        return OK;
    }

    pipe_t* data = file->data;
    if (data->readEnd != file)
    {
        return ERR(DRIVER, INVAL);
    }

    if (count >= PAGE_SIZE)
    {
        return ERR(DRIVER, INVAL);
    }

    LOCK_SCOPE(&data->lock);

    if (fifo_bytes_readable(&data->ring) == 0)
    {
        if (file->mode & MODE_NONBLOCK)
        {
            return ERR(DRIVER, AGAIN);
        }

        status_t status = WAIT_BLOCK_LOCK(&data->waitQueue, &data->lock,
            fifo_bytes_readable(&data->ring) != 0 || data->isWriteClosed);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    *bytesRead = fifo_read(&data->ring, buffer, count);
    wait_unblock(&data->waitQueue, WAIT_ALL, OK);
    return OK;
}

static status_t pipe_write(file_t* file, const void* buffer, size_t count, size_t* offset, size_t* bytesWritten)
{
    UNUSED(offset);

    pipe_t* data = file->data;
    if (data->writeEnd != file)
    {
        return ERR(DRIVER, INVAL);
    }

    if (count >= PAGE_SIZE)
    {
        return ERR(DRIVER, INVAL);
    }

    LOCK_SCOPE(&data->lock);

    if (fifo_bytes_writeable(&data->ring) == 0)
    {
        if (file->mode & MODE_NONBLOCK)
        {
            return ERR(DRIVER, AGAIN);
        }

        status_t status = WAIT_BLOCK_LOCK(&data->waitQueue, &data->lock,
            fifo_bytes_writeable(&data->ring) != 0 || data->isReadClosed);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    if (data->isReadClosed)
    {
        wait_unblock(&data->waitQueue, WAIT_ALL, OK);
        return ERR(DRIVER, IO);
    }

    *bytesWritten = fifo_write(&data->ring, buffer, count);
    wait_unblock(&data->waitQueue, WAIT_ALL, OK);
    return OK;
}

static status_t pipe_poll(file_t* file, poll_events_t* revents, wait_queue_t** queue)
{
    pipe_t* data = file->data;
    LOCK_SCOPE(&data->lock);

    if (fifo_bytes_readable(&data->ring) != 0 || data->isWriteClosed)
    {
        *revents |= POLLIN;
    }
    if (fifo_bytes_writeable(&data->ring) > 0 || data->isReadClosed)
    {
        *revents |= POLLOUT;
    }
    if ((file == data->readEnd && data->isWriteClosed) || (file == data->writeEnd && data->isReadClosed))
    {
        *revents |= POLLHUP;
    }

    *queue = &data->waitQueue;
    return OK;
}

static file_ops_t fileOps = {
    .open = pipe_open,
    .open2 = pipe_open2,
    .close = pipe_close,
    .read = pipe_read,
    .write = pipe_write,
    .poll = pipe_poll,
};

status_t pipe_init(void)
{
    pipeDir = devfs_dir_new(NULL, "pipe", NULL, NULL);
    if (pipeDir == NULL)
    {
        LOG_ERR("failed to initialize pipe directory");
        return ERR(DRIVER, IO);
    }

    newFile = devfs_file_new(pipeDir, "new", NULL, &fileOps, NULL);
    if (newFile == NULL)
    {
        UNREF(pipeDir);
        LOG_ERR("failed to initialize pipe new file");
        return ERR(DRIVER, IO);
    }

    return OK;
}

void pipe_deinit(void)
{
    UNREF(newFile);
    newFile = NULL;
    UNREF(pipeDir);
    pipeDir = NULL;
}

/** @} */

status_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        return pipe_init();
    case MODULE_EVENT_UNLOAD:
        pipe_deinit();
        break;
    default:
        break;
    }

    return OK;
}

MODULE_INFO("Pipes", "Kai Norberg", "Implements pipes for inter-process communication", OS_VERSION, "MIT",
    "BOOT_ALWAYS");