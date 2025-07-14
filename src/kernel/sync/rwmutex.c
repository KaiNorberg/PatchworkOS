#include "sync/rwmutex.h"

#include "sched/wait.h"
#include "sync/lock.h"

#include <assert.h>
#include <errno.h>

void rwmutex_init(rwmutex_t* mtx)
{
    mtx->activeReaders = 0;
    mtx->waitingWriters = 0;
    mtx->isWriterActive = false;
    wait_queue_init(&mtx->readerQueue);
    wait_queue_init(&mtx->writerQueue);
    lock_init(&mtx->lock);
}

void rwmutex_deinit(rwmutex_t* mtx)
{
    assert(mtx->activeReaders == 0);
    assert(mtx->waitingWriters == 0);
    assert(!mtx->isWriterActive);
    wait_queue_deinit(&mtx->readerQueue);
    wait_queue_deinit(&mtx->writerQueue);
}

void rwmutex_read_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return;
    }

    LOCK_SCOPE(&mtx->lock);

    while (
        WAIT_BLOCK_LOCK(&mtx->readerQueue, &mtx->lock, !(mtx->isWriterActive || mtx->waitingWriters > 0)) != WAIT_NORM)
    {
        // Do nothing.
    }

    mtx->activeReaders++;
}

uint64_t rwmutex_read_try_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&mtx->lock);

    if (mtx->isWriterActive || mtx->waitingWriters > 0)
    {
        errno = EWOULDBLOCK;
        return ERR;
    }

    mtx->activeReaders++;
    return 0;
}

void rwmutex_read_release(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return;
    }

    LOCK_SCOPE(&mtx->lock);

    mtx->activeReaders--;

    if (mtx->activeReaders == 0 && mtx->waitingWriters > 0)
    {
        wait_unblock(&mtx->writerQueue, 1);
    }
}

void rwmutex_write_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return;
    }

    LOCK_SCOPE(&mtx->lock);

    mtx->waitingWriters++;

    while (
        WAIT_BLOCK_LOCK(&mtx->writerQueue, &mtx->lock, !(mtx->activeReaders > 0 || mtx->isWriterActive)) != WAIT_NORM)
    {
        // Do nothing.
    }

    mtx->waitingWriters--;
    mtx->isWriterActive = true;
}

uint64_t rwmutex_write_try_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&mtx->lock);

    if (mtx->activeReaders > 0 || mtx->isWriterActive)
    {
        errno = EWOULDBLOCK;
        return ERR;
    }

    mtx->isWriterActive = true;
    return 0;
}

void rwmutex_write_release(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return;
    }

    LOCK_SCOPE(&mtx->lock);

    mtx->isWriterActive = false;

    if (mtx->waitingWriters > 0)
    {
        wait_unblock(&mtx->writerQueue, 1);
    }
    else
    {
        wait_unblock(&mtx->readerQueue, WAIT_ALL);
    }
}
