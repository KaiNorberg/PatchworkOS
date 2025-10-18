#include "sync/rwmutex.h"

#include "sched/thread.h"
#include "sched/wait.h"
#include "sync/lock.h"
#include "log/panic.h"

#include <assert.h>
#include <errno.h>

void rwmutex_ctx_init(rwmutex_ctx_t* ctx)
{
    for (uint64_t i = 0; i < RWMUTEX_MAX_MUTEXES; i++)
    {
        ctx->entries[i].mutex = NULL;
        ctx->entries[i].readDepth = 0;
        ctx->entries[i].writeDepth = 0;
    }
}

void rwmutex_ctx_deinit(rwmutex_ctx_t* ctx)
{
    for (uint64_t i = 0; i < RWMUTEX_MAX_MUTEXES; i++)
    {
        ctx->entries[i].mutex = NULL;
        ctx->entries[i].readDepth = 0;
        ctx->entries[i].writeDepth = 0;
    }
}

static rwmutex_ctx_entry_t* rwmutex_ctx_get_entry(rwmutex_ctx_t* ctx, rwmutex_t* mtx)
{
    for (uint64_t i = 0; i < RWMUTEX_MAX_MUTEXES; i++)
    {
        if (ctx->entries[i].mutex == mtx)
        {
            return &ctx->entries[i];
        }
    }

    for (uint64_t i = 0; i < RWMUTEX_MAX_MUTEXES; i++)
    {
        if (ctx->entries[i].mutex == NULL)
        {
            ctx->entries[i].mutex = mtx;
            return &ctx->entries[i];
        }
    }

    panic(NULL, "Thread exceeded maximum rwmutex acquire count");
}

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

    rwmutex_ctx_entry_t* ctxEntry = rwmutex_ctx_get_entry(&sched_thread()->rwmutexCtx, mtx);
    if (ctxEntry->writeDepth > 0 || ctxEntry->readDepth > 0)
    {
        ctxEntry->readDepth++;
        return;
    }

    LOCK_SCOPE(&mtx->lock);

    while (WAIT_BLOCK_LOCK(&mtx->readerQueue, &mtx->lock, !(mtx->hasWriter || mtx->waitingWriters > 0)) == ERR)
    {
        // Wait until unblocked.
    }

    mtx->activeReaders++;
    ctxEntry->readDepth++;
}

uint64_t rwmutex_read_try_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    rwmutex_ctx_entry_t* ctxEntry = rwmutex_ctx_get_entry(&sched_thread()->rwmutexCtx, mtx);
    if (ctxEntry->writeDepth > 0 || ctxEntry->readDepth > 0)
    {
        ctxEntry->readDepth++;
        return 0;
    }

    LOCK_SCOPE(&mtx->lock);

    if (mtx->waitingWriters > 0 || mtx->hasWriter)
    {
        errno = EWOULDBLOCK;
        return ERR;
    }

    mtx->activeReaders++;
    ctxEntry->readDepth++;
    return 0;
}

void rwmutex_read_release(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return;
    }

    rwmutex_ctx_entry_t* ctxEntry = rwmutex_ctx_get_entry(&sched_thread()->rwmutexCtx, mtx);
    assert(ctxEntry->readDepth > 0);
    ctxEntry->readDepth--;
    if (ctxEntry->writeDepth > 0 || ctxEntry->readDepth > 0)
    {
        return;
    }

    ctxEntry->mutex = NULL;

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

    rwmutex_ctx_entry_t* ctxEntry = rwmutex_ctx_get_entry(&sched_thread()->rwmutexCtx, mtx);
    if (ctxEntry->writeDepth > 0)
    {
        ctxEntry->writeDepth++;
        return;
    }

    if (ctxEntry->readDepth > 0)
    {
        panic(NULL, "rwmutex does not support upgrading from read to write lock");
    }

    LOCK_SCOPE(&mtx->lock);

    mtx->waitingWriters++;
    while (WAIT_BLOCK_LOCK(&mtx->writerQueue, &mtx->lock, !(mtx->activeReaders > 0 || mtx->hasWriter)) == ERR)
    {
        // Wait until unblocked.
    }

    mtx->waitingWriters--;
    mtx->hasWriter = true;
    ctxEntry->writeDepth++;
}

uint64_t rwmutex_write_try_acquire(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    rwmutex_ctx_entry_t* ctxEntry = rwmutex_ctx_get_entry(&sched_thread()->rwmutexCtx, mtx);
    if (ctxEntry->writeDepth > 0)
    {
        ctxEntry->writeDepth++;
        return 0;
    }

    if (ctxEntry->readDepth > 0)
    {
        panic(NULL, "rwmutex does not support upgrading from read to write lock");
    }

    LOCK_SCOPE(&mtx->lock);

    if (mtx->activeReaders > 0 || mtx->hasWriter)
    {
        errno = EWOULDBLOCK;
        return ERR;
    }

    mtx->hasWriter = true;
    ctxEntry->writeDepth++;
    return 0;
}

void rwmutex_write_acquire_spin(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return;
    }

    rwmutex_ctx_entry_t* ctxEntry = rwmutex_ctx_get_entry(&sched_thread()->rwmutexCtx, mtx);
    if (ctxEntry->writeDepth > 0)
    {
        ctxEntry->writeDepth++;
        return;
    }

    if (ctxEntry->readDepth > 0)
    {
        panic(NULL, "rwmutex does not support upgrading from read to write lock");
    }

    while (true)
    {
        LOCK_SCOPE(&mtx->lock);

        if (mtx->activeReaders == 0 && !mtx->hasWriter)
        {
            mtx->hasWriter = true;
            ctxEntry->writeDepth++;
            return;
        }

        asm volatile("pause");
    }
}

void rwmutex_write_release(rwmutex_t* mtx)
{
    if (mtx == NULL)
    {
        return;
    }

    rwmutex_ctx_entry_t* ctxEntry = rwmutex_ctx_get_entry(&sched_thread()->rwmutexCtx, mtx);
    assert(ctxEntry->writeDepth > 0);
    ctxEntry->writeDepth--;
    if (ctxEntry->writeDepth > 0)
    {
        return;
    }

    if (ctxEntry->readDepth > 0)
    {
        LOCK_SCOPE(&mtx->lock);
        assert(mtx->hasWriter);
        mtx->hasWriter = false;

        wait_unblock(&mtx->readerQueue, WAIT_ALL, EOK);
        return;
    }

    ctxEntry->mutex = NULL;

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
