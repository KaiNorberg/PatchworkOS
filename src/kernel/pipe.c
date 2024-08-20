#include "pipe.h"

#include "lock.h"
#include "pmm.h"
#include "sched.h"
#include "vfs.h"

#include <stdlib.h>

// TODO: Optimize this, it is truly awful, create ring struct?

static void pipe_private_free(pipe_private_t* private)
{
    pmm_free(private->buffer);
    blocker_cleanup(&private->blocker);
    free(private);
}

static uint64_t pipe_read(file_t* file, void* buffer, uint64_t count)
{
    pipe_private_t* private = file->private;

    for (uint64_t i = 0; i < count; i++)
    {
        if (SCHED_BLOCK_LOCK(&private->blocker, &private->lock, private->readIndex != private->writeIndex) != BLOCK_NORM)
        {
            lock_release(&private->lock);
            return i;
        }

        ((uint8_t*)buffer)[i] = ((uint8_t*)private->buffer)[private->readIndex];
        private->readIndex = (private->readIndex + 1) % PAGE_SIZE;

        lock_release(&private->lock);
        sched_unblock(&private->blocker);
    }

    return count;
}

static uint64_t pipe_write(file_t* file, const void* buffer, uint64_t count)
{
    pipe_private_t* private = file->private;

    for (uint64_t i = 0; i < count; i++)
    {
        if (SCHED_BLOCK_LOCK(&private->blocker, &private->lock, (private->writeIndex + 1) % PAGE_SIZE != private->readIndex) !=
            BLOCK_NORM)
        {
            lock_release(&private->lock);
            return i;
        }

        ((uint8_t*)private->buffer)[private->writeIndex] = ((uint8_t*)buffer)[i];
        private->writeIndex = (private->writeIndex + 1) % PAGE_SIZE;

        lock_release(&private->lock);
        sched_unblock(&private->blocker);
    }

    return count;
}

static void pipe_read_cleanup(file_t* file)
{
    pipe_private_t* private = file->private;
    lock_acquire(&private->lock);

    private->readClosed = true;
    if (private->writeClosed)
    {
        lock_release(&private->lock);
        pipe_private_free(private);
        return;
    }

    lock_release(&private->lock);
}

static void pipe_write_cleanup(file_t* file)
{
    pipe_private_t* private = file->private;
    lock_acquire(&private->lock);

    private->writeClosed = true;
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
    .cleanup = pipe_read_cleanup,
};

static file_ops_t writeOps = {
    .write = pipe_write,
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
    private->buffer = pmm_alloc();
    private->readClosed = false;
    private->writeClosed = false;
    private->readIndex = 0;
    private->writeIndex = 0;
    blocker_init(&private->blocker);
    lock_init(&private->lock);

    pipe->read->private = private;
    pipe->write->private = private;

    return 0;
}
