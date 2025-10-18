#include "sync/rwmutex.h"

#include "log/panic.h"
#include "sched/wait.h"
#include "sync/lock.h"

#include <assert.h>
#include <errno.h>

void rwmutex_init(rwmutex_t* mtx)
{
    mtx->activeReaders = 0;
    mtx->waitingWriters = 0;
    mtx->hasWriter = false;
    wait_queue_init(&mtx->readerQueue);
    wait_queue_init(&mtx->writerQueue);
    lock_init(&mtx->lock);
}

void rwmutex_deinit(rwmutex_t* mtx)
{
    LOCK_SCOPE(&mtx->lock);
    assert(mtx->activeReaders == 0);
    assert(mtx->waitingWriters == 0);
    assert(!mtx->hasWriter);
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

    while (WAIT_BLOCK_LOCK(&mtx->readerQueue, &mtx->lock, !(mtx->hasWriter || mtx->waitingWriters > 0)) == ERR)
    {
        // Wait until unblocked.
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

    if (mtx->waitingWriters > 0 || mtx->hasWriter)
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

    assert(mtx->activeReaders > 0);
    mtx->activeReaders--;

    if (mtx->activeReaders == 0 && mtx->waitingWriters > 0)
    {
        wait_unblock(&mtx->writerQueue, 1, EOK);
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
    while (WAIT_BLOCK_LOCK(&mtx->writerQueue, &mtx->lock, !(mtx->activeReaders > 0 || mtx->hasWriter)) == ERR)
    {
        // Wait until unblocked
    }

    mtx->waitingWriters--;
    mtx->hasWriter = true;
}

uint64_t rwmutex_write_try_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&mtx->lock);

    if (mtx->activeReaders > 0 || mtx->hasWriter)
    {
        errno = EWOULDBLOCK;
        return ERR;
    }

    mtx->hasWriter = true;
    return 0;
}

void rwmutex_write_release(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return;
    }

    LOCK_SCOPE(&mtx->lock);

    mtx->hasWriter = false;

    if (mtx->waitingWriters > 0)
    {
        wait_unblock(&mtx->writerQueue, 1, EOK);
    }
    else
    {
        wait_unblock(&mtx->readerQueue, WAIT_ALL, EOK);
    }
}
