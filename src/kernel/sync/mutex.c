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
    lock_init(&mtx->lock);
}

void mutex_deinit(mutex_t* mtx)
{
    assert(mtx->owner == NULL);
    wait_queue_deinit(&mtx->waitQueue);
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
    lock_release(&mtx->lock);
}

void mutex_release(mutex_t* mtx)
{
    assert(mtx != NULL);
    assert(mtx->owner == sched_thread());

    LOCK_SCOPE(&mtx->lock);

    mtx->owner = NULL;

    wait_unblock(&mtx->waitQueue, 1);
}
