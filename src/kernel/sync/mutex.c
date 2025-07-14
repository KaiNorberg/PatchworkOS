#include "sync/mutex.h"

#include "sched/wait.h"
#include "sync/lock.h"

#include <assert.h>
#include <errno.h>

void mutex_init(mutex_t* mtx)
{
    lock_init(&mtx->lock);
    wait_queue_init(&mtx->waitQueue);
    mtx->isAcquired = false;
}

void mutex_deinit(mutex_t* mtx)
{
    assert(!mtx->isAcquired);
    wait_queue_deinit(&mtx->waitQueue);
}

void mutex_acquire(mutex_t* mtx)
{
    LOCK_SCOPE(&mtx->lock);

    while (WAIT_BLOCK_LOCK(&mtx->waitQueue, &mtx->lock, !mtx->isAcquired) != WAIT_NORM)
    {
        // Do nothing.
    }

    mtx->isAcquired = true;
}

void mutex_release(mutex_t* mtx)
{
    LOCK_SCOPE(&mtx->lock);

    mtx->isAcquired = false;

    wait_unblock(&mtx->waitQueue, 1);
}
