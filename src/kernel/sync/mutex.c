#include "sync/mutex.h"

#include "config.h"
#include "sched/sched.h"
#include "sched/thread.h"
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

void mutex_acquire_recursive(mutex_t* mtx)
{
    assert(mtx != NULL);
    thread_t* self = sched_thread();
    assert(self != NULL);

    lock_acquire(&mtx->lock);
    if (mtx->owner == self)
    {
        mtx->depth++;
        lock_release(&mtx->lock);
        return;
    }
    lock_release(&mtx->lock);

    mutex_acquire(mtx);
}

void mutex_acquire(mutex_t* mtx)
{
    assert(mtx != NULL);
    thread_t* self = sched_thread();
    assert(self != NULL);
    assert(mtx->owner != self);

    uint64_t spin = 0;
    while (true)
    {
        lock_acquire(&mtx->lock);

        if (mtx->owner == NULL)
        {
            mtx->owner = self;
            mtx->depth = 1;
            lock_release(&mtx->lock);
            return;
        }

        if (spin >= CONFIG_MUTEX_MAX_SLOW_SPIN)
        {
            break;
        }

        lock_release(&mtx->lock);
        spin++;
    }

    while (WAIT_BLOCK_LOCK(&mtx->waitQueue, &mtx->lock, mtx->owner == NULL) != WAIT_NORM)
    {
        // Do nothing.
    }

    mtx->owner = self;
    mtx->depth = 1;
    lock_release(&mtx->lock);
}

void mutex_release(mutex_t* mtx)
{
    assert(mtx != NULL);
    LOCK_SCOPE(&mtx->lock);

    assert(mtx->owner == sched_thread());
    assert(mtx->depth > 0);

    mtx->depth--;
    if (mtx->depth == 0)
    {
        mtx->owner = NULL;
        wait_unblock(&mtx->waitQueue, 1);
    }
}
