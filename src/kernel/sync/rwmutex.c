#include <kernel/sync/rwmutex.h>

#include <kernel/log/panic.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

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

    RETRY(WAIT_BLOCK_LOCK(&mtx->readerQueue, &mtx->lock, !(mtx->hasWriter || mtx->waitingWriters > 0)));

    mtx->activeReaders++;
}

bool rwmutex_read_try_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return false;
    }

    LOCK_SCOPE(&mtx->lock);

    if (mtx->waitingWriters > 0 || mtx->hasWriter)
    {
        return false;
    }

    mtx->activeReaders++;
    return true;
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
    RETRY(WAIT_BLOCK_LOCK(&mtx->writerQueue, &mtx->lock, !(mtx->activeReaders > 0 || mtx->hasWriter)));

    mtx->waitingWriters--;
    mtx->hasWriter = true;
}

bool rwmutex_write_try_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return false;
    }

    LOCK_SCOPE(&mtx->lock);

    if (mtx->activeReaders > 0 || mtx->hasWriter)
    {
        return false;
    }

    mtx->hasWriter = true;
    return true;
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
