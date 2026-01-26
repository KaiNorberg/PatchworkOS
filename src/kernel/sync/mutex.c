#include <kernel/sync/mutex.h>

#include <kernel/config.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

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
    UNUSED(isAcquired);
}

bool mutex_acquire_timeout(mutex_t* mtx, clock_t timeout)
{
    assert(mtx != NULL);
    thread_t* self = thread_current();
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
        status_t status = WAIT_BLOCK_LOCK(&mtx->waitQueue, &mtx->lock, mtx->owner == NULL);
        if (IS_ERR(status))
        {
            lock_release(&mtx->lock);
            return false;
        }
        mtx->owner = self;
        mtx->depth = 1;
        lock_release(&mtx->lock);
        return true;
    }

    lock_acquire(&mtx->lock);
    clock_t end = clock_uptime() + timeout;
    while (true)
    {
        clock_t now = clock_uptime();
        if (now >= end)
        {
            lock_release(&mtx->lock);
            return false;
        }

        clock_t waitTime = end - now;
        status_t status = WAIT_BLOCK_LOCK_TIMEOUT(&mtx->waitQueue, &mtx->lock, mtx->owner == NULL, waitTime);
        if (IS_ERR(status))
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

    assert(mtx->owner == thread_current_unsafe());

    mtx->depth--;
    if (mtx->depth == 0)
    {
        mtx->owner = NULL;
        wait_unblock(&mtx->waitQueue, 1, EOK);
    }
}
