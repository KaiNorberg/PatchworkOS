#include <kernel/sync/rcu.h>
#include <kernel/cpu/cpu.h>

typedef struct
{
    rcu_head_t head;
    wait_queue_t wait;
    lock_t lock;
    bool done;
} rcu_synchronize_t;

static void rcu_synchronize_callback(void* arg)
{
    rcu_synchronize_t* sync = (rcu_synchronize_t*)arg;
    lock_acquire(&sync->lock);
    sync->done = true;
    wait_unblock(&sync->wait, WAIT_ALL, EOK);
    lock_release(&sync->lock);
}

void rcu_synchronize(void)
{
    rcu_synchronize_t sync;
    wait_queue_init(&sync.wait);
    lock_init(&sync.lock);
    sync.done = false;

    rcu_call(&sync.head, rcu_synchronize_callback);

    lock_acquire(&sync.lock);
    WAIT_BLOCK_LOCK(&sync.wait, &sync.lock, sync.done);
    lock_release(&sync.lock);

    wait_queue_deinit(&sync.wait);
}

void rcu_call(rcu_head_t* head, rcu_callback_t func)
{
    INTERRUPT_SCOPE();

    cpu_t* self = cpu_get();

    head->func = func;
    head->next = self->rcu.head;
    self->rcu.head = head;
}