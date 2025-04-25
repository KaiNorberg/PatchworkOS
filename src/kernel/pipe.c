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
        return ERROR(ENOOP);
    }

    if (count >= PAGE_SIZE)
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

    ASSERT_PANIC(ring_read(&private->ring, buffer, count) != ERR);

    lock_release(&private->lock);
    waitsys_unblock(&private->waitQueue, WAITSYS_ALL);
    return count;
}

static uint64_t pipe_write(file_t* file, const void* buffer, uint64_t count)
{
    pipe_private_t* private = file->private;
    if (private->writeEnd != file)
    {
        return ERROR(ENOOP);
    }

    if (count >= PAGE_SIZE)
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
        waitsys_unblock(&private->waitQueue, WAITSYS_ALL);
        return ERROR(EPIPE);
    }

    ASSERT_PANIC(ring_write(&private->ring, buffer, count) != ERR);

    lock_release(&private->lock);
    waitsys_unblock(&private->waitQueue, WAITSYS_ALL);
    return count;
}

static wait_queue_t* pipe_poll(file_t* file, poll_file_t* pollFile)
{
    pipe_private_t* private = file->private;

    pollFile->occurred = (POLL_READ & (ring_data_length(&private->ring) != 0 || private->writeClosed)) |
        (POLL_WRITE & (ring_free_length(&private->ring) != 0 || private->readClosed));
    return &private->waitQueue;
}

static file_ops_t fileOps = {
    .read = pipe_read,
    .write = pipe_write,
    .poll = pipe_poll,
};

static file_t* pipe_open(volume_t* volume, sysobj_t* sysobj)
{
    pipe_private_t* private = malloc(sizeof(pipe_private_t));
    if (private == NULL)
    {
        return NULL;
    }
    private->buffer = pmm_alloc();
    if (private->buffer == NULL)
    {
        free(private);
        return NULL;
    }
    ring_init(&private->ring, private->buffer, PAGE_SIZE);
    private->readClosed = false;
    private->writeClosed = false;
    wait_queue_init(&private->waitQueue);
    lock_init(&private->lock);

    file_t* file = file_new(volume);
    if (file == NULL)
    {
        free(private->buffer);
        free(private);
        return NULL;
    }
    file->ops = &fileOps;

    private->readEnd = file;
    private->writeEnd = file;

    file->private = private;
    return file;
}

static uint64_t pipe_open2(volume_t* volume, sysobj_t* sysobj, file_t* files[2])
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
    private->readClosed = false;
    private->writeClosed = false;
    wait_queue_init(&private->waitQueue);
    lock_init(&private->lock);

    files[0] = file_new(volume);
    if (files[0] == NULL)
    {
        free(private->buffer);
        free(private);
        return ERR;
    }
    files[0]->ops = &fileOps;

    files[1] = file_new(volume);
    if (files[1] == NULL)
    {
        free(private->buffer);
        free(private);
        return ERR;
    }
    files[1]->ops = &fileOps;

    private->readEnd = files[PIPE_READ];
    private->writeEnd = files[PIPE_WRITE];

    files[0]->private = private;
    files[1]->private = private;

    return 0;
}

static void pipe_cleanup(sysobj_t* sysobj, file_t* file)
{
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

    waitsys_unblock(&private->waitQueue, WAITSYS_ALL);
    if (private->writeClosed && private->readClosed)
    {
        lock_release(&private->lock);
        wait_queue_deinit(&private->waitQueue);
        free(private->buffer);
        free(private);
        return;
    }

    lock_release(&private->lock);
}

static sysobj_ops_t resourceOps = {
    .open = pipe_open,
    .open2 = pipe_open2,
    .cleanup = pipe_cleanup,
};

void pipe_init(void)
{
    sysobj_new("/pipe", "new", &resourceOps, NULL);
}
