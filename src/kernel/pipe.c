#include "pipe.h"

#include "lock.h"
#include "log.h"
#include "pmm.h"
#include "ring.h"
#include "sched.h"
#include "vfs.h"

#include <stdlib.h>
#include <sys/math.h>

static void pipe_private_free(pipe_private_t* private)
{
    ring_deinit(&private->ring);
    wait_queue_deinit(&private->waitQueue);
    free(private);
}

static uint64_t pipe_read(file_t* file, void* buffer, uint64_t count)
{
    pipe_private_t* private = file->private;

    if (count >= RING_SIZE)
    {
        return ERROR(EINVAL);
    }

    if (WAITSYS_BLOCK_LOCK(&private->waitQueue, &private->lock,
            ring_data_length(&private->ring) >= count || private->writeClosed) != BLOCK_NORM)
    {
        lock_release(&private->lock);
        return 0;
    }

    if (private->writeClosed)
    {
        count = MIN(count, ring_data_length(&private->ring));
    }

    ASSERT_PANIC(ring_read(&private->ring, buffer, count) != ERR, "ring_read");

    lock_release(&private->lock);
    waitsys_unblock(&private->waitQueue);
    return count;
}

static uint64_t pipe_write(file_t* file, const void* buffer, uint64_t count)
{
    pipe_private_t* private = file->private;

    if (count >= RING_SIZE)
    {
        return ERROR(EINVAL);
    }

    if (WAITSYS_BLOCK_LOCK(&private->waitQueue, &private->lock,
            ring_free_length(&private->ring) >= count || private->readClosed) != BLOCK_NORM)
    {
        lock_release(&private->lock);
        return 0;
    }

    if (private->readClosed)
    {
        lock_release(&private->lock);
        waitsys_unblock(&private->waitQueue);
        return ERROR(EPIPE);
    }

    ASSERT_PANIC(ring_write(&private->ring, buffer, count) != ERR, "ring_write");

    lock_release(&private->lock);
    waitsys_unblock(&private->waitQueue);
    return count;
}

static wait_queue_t* pipe_read_poll(file_t* file, poll_file_t* pollFile)
{
    pipe_private_t* private = file->private;

    pollFile->occurred = POLL_READ & (ring_data_length(&private->ring) != 0 || private->writeClosed);
    return &private->waitQueue;
}

static wait_queue_t* pipe_write_poll(file_t* file, poll_file_t* pollFile)
{
    pipe_private_t* private = file->private;

    pollFile->occurred = POLL_WRITE & (ring_free_length(&private->ring) != 0 || private->readClosed);
    return &private->waitQueue;
}

static void pipe_read_cleanup(file_t* file)
{
    pipe_private_t* private = file->private;
    lock_acquire(&private->lock);

    private->readClosed = true;
    waitsys_unblock(&private->waitQueue);
    if (private->writeClosed)
    {
        lock_release(&private->lock);
        pipe_private_free(private);
        return;
    }

    lock_release(&private->lock);
}

#include <stdio.h>

static void pipe_write_cleanup(file_t* file)
{
    pipe_private_t* private = file->private;
    lock_acquire(&private->lock);

    private->writeClosed = true;
    waitsys_unblock(&private->waitQueue);
    if (private->readClosed)
    {
        lock_release(&private->lock);
        pipe_private_free(private);
        return;
    }

    lock_release(&private->lock);
}

static file_ops_t readOps = {
    .read = pipe_read,
    .poll = pipe_read_poll,
    .cleanup = pipe_read_cleanup,
};

static file_ops_t writeOps = {
    .write = pipe_write,
    .poll = pipe_write_poll,
    .cleanup = pipe_write_cleanup,
};

uint64_t pipe_init(pipe_file_t* pipe)
{
    pipe->read = file_new(NULL);
    if (pipe->read == NULL)
    {
        return ERR;
    }
    pipe->read->ops = &readOps;

    pipe->write = file_new(NULL);
    if (pipe->write == NULL)
    {
        file_deref(pipe->read);
        return ERR;
    }
    pipe->write->ops = &writeOps;

    pipe_private_t* private = malloc(sizeof(pipe_private_t));
    if (private == NULL)
    {
        file_deref(pipe->read);
        file_deref(pipe->write);
        return ERR;
    }
    ring_init(&private->ring);
    private->readClosed = false;
    private->writeClosed = false;
    wait_queue_init(&private->waitQueue);
    lock_init(&private->lock);

    pipe->read->private = private;
    pipe->write->private = private;

    return 0;
}
