#include "sync/mutex.h"

#include "config.h"
#include "sched/sched.h"
#include "sched/thread.h"
#include "sched/timer.h"
#include "sched/wait.h"
#include "sync/lock.h"

#include <assert.h>

void mutex_init(mutex_t* mtx)
{
    wait_queue_init(&mtx->waitQueue);
    mtx->owner = NULL;
    mtx->depth = 0;
    lock_init(&mtx->lock);
}

void mutex_deinit(mutex_t* mtx)
{
    assert(mtx->owner == NULL);
    wait_queue_deinit(&mtx->waitQueue);
}

void mutex_acquire(mutex_t* mtx)
{
    bool isAcquired = mutex_acquire_timeout(mtx, CLOCKS_NEVER);
    assert(isAcquired);
    (void)isAcquired;
}

bool mutex_acquire_timeout(mutex_t* mtx, clock_t timeout)
{
    assert(mtx != NULL);
    thread_t* self = sched_thread();
    assert(self != NULL);

    if (mtx->owner == self)
    {
        mtx->depth++;
        return true;
    }

    uint64_t spin = 0;
    while (spin < CONFIG_MUTEX_MAX_SLOW_SPIN)
    {
        lock_acquire(&mtx->lock);
        if (mtx->owner == NULL)
        {
            mtx->owner = self;
            mtx->depth = 1;
            lock_release(&mtx->lock);
            return true;
        }
        lock_release(&mtx->lock);
        spin++;
    }

    if (timeout == 0)
    {
        return false;
    }

    if (timeout == CLOCKS_NEVER)
    {
        lock_acquire(&mtx->lock);
        while (WAIT_BLOCK_LOCK(&mtx->waitQueue, &mtx->lock, mtx->owner == NULL) == ERR)
        {
        }

        mtx->owner = self;
        mtx->depth = 1;
        lock_release(&mtx->lock);
        return true;
    }

    lock_acquire(&mtx->lock);
    clock_t end = timer_uptime() + timeout;
    while (true)
    {
        clock_t now = timer_uptime();
        if (now >= end)
        {
            lock_release(&mtx->lock);
            return false;
        }

        clock_t waitTime = end - now;
        if (WAIT_BLOCK_LOCK_TIMEOUT(&mtx->waitQueue, &mtx->lock, mtx->owner == NULL, waitTime) == ERR)
        {
            lock_release(&mtx->lock);
            return false;
        }

        mtx->owner = self;
        mtx->depth = 1;
        lock_release(&mtx->lock);
        return true;
    }
}

void mutex_release(mutex_t* mtx)
{
    assert(mtx != NULL);
    LOCK_SCOPE(&mtx->lock);

    assert(mtx->owner == sched_thread());

    mtx->depth--;
    if (mtx->depth == 0)
    {
        mtx->owner = NULL;
        wait_unblock(&mtx->waitQueue, 1, EOK);
    }
}
