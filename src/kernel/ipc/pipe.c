#include "pipe.h"

#include "fs/file.h"
#include "fs/vfs.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "utils/ring.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/io.h>
#include <sys/math.h>

static sysfs_file_t pipeFile;

static uint64_t pipe_open(file_t* file)
{
    pipe_private_t* private = heap_alloc(sizeof(pipe_private_t), HEAP_NONE);
    if (private == NULL)
    {
        return ERR;
    }
    private->buffer = pmm_alloc();
    if (private->buffer == NULL)
    {
        heap_free(private);
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
    pipe_private_t* private = heap_alloc(sizeof(pipe_private_t), HEAP_NONE);
    if (private == NULL)
    {
        return ERR;
    }
    private->buffer = pmm_alloc();
    if (private->buffer == NULL)
    {
        heap_free(private);
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

    wait_unblock(&private->waitQueue, WAIT_ALL);
    if (private->isWriteClosed && private->isReadClosed)
    {
        lock_release(&private->lock);
        wait_queue_deinit(&private->waitQueue);
        pmm_free(private->buffer);
        heap_free(private);
        return;
    }

    lock_release(&private->lock);
    LOG_DEBUG("cleanup\n");
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
            ring_data_length(&private->ring) != 0 || private->isWriteClosed) != WAIT_NORM)
    {
        return 0;
    }

    count = MIN(count, ring_data_length(&private->ring));
    if (ring_read(&private->ring, buffer, count) == ERR)
    {
        panic(NULL, "Failed to read from pipe");
    }

    wait_unblock(&private->waitQueue, WAIT_ALL);

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
            ring_free_length(&private->ring) >= count || private->isReadClosed) != WAIT_NORM)
    {
        errno = EINTR;
        return ERR;
    }

    if (private->isReadClosed)
    {
        wait_unblock(&private->waitQueue, WAIT_ALL);
        errno = EPIPE;
        return ERR;
    }

    if (ring_write(&private->ring, buffer, count) == ERR)
    {
        panic(NULL, "Failed to write to pipe");
    }

    wait_unblock(&private->waitQueue, 1);

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

void pipe_init(void)
{
    if (sysfs_file_init(&pipeFile, sysfs_get_default(), "pipe", NULL, &fileOps, NULL) == ERR)
    {
        panic(NULL, "Failed to initialize pipe file");
    }
}
