#include "pipe.h"

#include "fs/vfs.h"
#include "log/log.h"
#include "mem/heap.h"
#include "mem/pmm.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "utils/ring.h"

#include <assert.h>
#include <stdlib.h>
#include <sys/math.h>

static sysobj_t obj;

static uint64_t pipe_read(file_t* file, void* buffer, uint64_t count)
{
    if (count == 0)
    {
        return 0;
    }

    pipe_private_t* private = file->private;
    if (private->readEnd != file)
    {
        return ERROR(ENOTSUP);
    }

    if (count >= PAGE_SIZE)
    {
        return ERROR(EINVAL);
    }

    LOCK_DEFER(&private->lock);

    if (WAIT_BLOCK_LOCK(&private->waitQueue, &private->lock,
            ring_data_length(&private->ring) != 0 || private->isWriteClosed) != WAIT_NORM)
    {
        return 0;
    }

    count = MIN(count, ring_data_length(&private->ring));
    assert(ring_read(&private->ring, buffer, count) != ERR);

    wait_unblock(&private->waitQueue, WAIT_ALL);
    return count;
}

static uint64_t pipe_write(file_t* file, const void* buffer, uint64_t count)
{
    pipe_private_t* private = file->private;
    if (private->writeEnd != file)
    {
        return ERROR(ENOTSUP);
    }

    if (count >= PAGE_SIZE)
    {
        return ERROR(EINVAL);
    }

    LOCK_DEFER(&private->lock);

    if (WAIT_BLOCK_LOCK(&private->waitQueue, &private->lock,
            ring_free_length(&private->ring) >= count || private->isReadClosed) != WAIT_NORM)
    {
        return ERROR(EINTR);
    }

    if (private->isReadClosed)
    {
        wait_unblock(&private->waitQueue, WAIT_ALL);
        return ERROR(EPIPE);
    }

    assert(ring_write(&private->ring, buffer, count) != ERR);

    wait_unblock(&private->waitQueue, 1);
    return count;
}

static wait_queue_t* pipe_poll(file_t* file, poll_file_t* pollFile)
{
    pipe_private_t* private = file->private;
    LOCK_DEFER(&private->lock);
    pollFile->revents = ((ring_data_length(&private->ring) != 0 || private->isWriteClosed) ? POLL_READ : 0) |
        ((ring_free_length(&private->ring) != 0 || private->isReadClosed) ? POLL_WRITE : 0);
    return &private->waitQueue;
}

static uint64_t pipe_open(inode_t* inode, file_t* file)
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

static uint64_t pipe_open2(inode_t* inode, file_t* files[2])
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

static void pipe_cleanup(file_t* file)
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
}

static file_ops_t fileOps = {
    .open = pipe_open,
    .open2 = pipe_open2,
    .read = pipe_read,
    .write = pipe_write,
    .poll = pipe_poll,
    .cleanup = pipe_cleanup,
};

void pipe_init(void)
{
    assert(sysobj_init_path(&obj, "/pipe", "new", &fileOps, NULL) != ERR);
}
