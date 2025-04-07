#include "pipe.h"

#include "lock.h"
#include "log.h"
#include "pmm.h"
#include "ring.h"
#include "sched.h"
#include "vfs.h"

#include <stdlib.h>
#include <sys/math.h>

static uint64_t pipe_read(file_t* file, void* buffer, uint64_t count)
{
    pipe_private_t* private = file->private;
    if (private->readEnd != file)
    {
        return ERROR(EACCES);
    }

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
    if (private->writeEnd != file)
    {
        return ERROR(EACCES);
    }

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

static wait_queue_t* pipe_poll(file_t* file, poll_file_t* pollFile)
{
    pipe_private_t* private = file->private;

    pollFile->occurred = (POLL_READ & (ring_data_length(&private->ring) != 0 || private->writeClosed)) |
        (POLL_WRITE & (ring_free_length(&private->ring) != 0 || private->readClosed));
    return &private->waitQueue;
}

static void pipe_cleanup(file_t* file)
{
    SYSFS_CLEANUP(file);

    pipe_private_t* private = file->private;
    lock_acquire(&private->lock);
    if (private->readEnd == file)
    {
        private->readClosed = true;
    }
    if (private->writeEnd == file)
    {
        private->writeClosed = true;
    }

    waitsys_unblock(&private->waitQueue);
    if (private->writeClosed && private->readClosed)
    {
        lock_release(&private->lock);
        ring_deinit(&private->ring);
        wait_queue_deinit(&private->waitQueue);
        free(private);
        return;
    }

    lock_release(&private->lock);
}

static file_ops_t fileOps = {
    .read = pipe_read,
    .write = pipe_write,
    .poll = pipe_poll,
    .cleanup = pipe_cleanup,
};

static file_t* pipe_open(volume_t* volume, resource_t* resource)
{
    file_t* file = file_new(volume);
    if (file == NULL)
    {
        return NULL;
    }
    file->ops = &fileOps;

    pipe_private_t* private = malloc(sizeof(pipe_private_t));
    if (private == NULL)
    {
        return NULL;
    }
    ring_init(&private->ring);
    private->readClosed = false;
    private->writeClosed = false;
    wait_queue_init(&private->waitQueue);
    lock_init(&private->lock);
    private->readEnd = file;
    private->writeEnd = file;

    file->private = private;
    return file;
}

static uint64_t pipe_open2(volume_t* volume, resource_t* resource, file_t* files[2])
{
    files[0] = file_new(volume);
    if (files[0] == NULL)
    {
        return ERR;
    }
    files[0]->ops = &fileOps;

    files[1] = file_new(volume);
    if (files[1] == NULL)
    {
        return ERR;
    }
    files[1]->ops = &fileOps;

    pipe_private_t* private = malloc(sizeof(pipe_private_t));
    if (private == NULL)
    {
        return ERR;
    }
    ring_init(&private->ring);
    private->readClosed = false;
    private->writeClosed = false;
    wait_queue_init(&private->waitQueue);
    lock_init(&private->lock);
    private->readEnd = files[PIPE_READ];
    private->writeEnd = files[PIPE_WRITE];

    files[0]->private = private;
    files[1]->private = private;

    return 0;
}

static resource_ops_t resOps = {
    .open = pipe_open,
    .open2 = pipe_open2,
};

void pipe_init(void)
{
    sysfs_expose("/pipe", "new", &resOps, NULL);
}
