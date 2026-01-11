#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/log/log.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rcu.h>
#include <stdlib.h>
#include <sys/bitmap.h>

static BITMAP_CREATE_ZERO(ack, CPU_MAX);
static uint64_t grace = 0;
static bool active = false;
static lock_t lock = LOCK_CREATE();

typedef struct
{
    rcu_entry_t rcu;
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

    rcu_call(&sync.rcu, rcu_synchronize_callback, &sync);

    lock_acquire(&sync.lock);
    WAIT_BLOCK_LOCK(&sync.wait, &sync.lock, sync.done);
    lock_release(&sync.lock);

    wait_queue_deinit(&sync.wait);
}

void rcu_call(rcu_entry_t* rcu, rcu_callback_t func, void* arg)
{
    if (rcu == NULL || func == NULL)
    {
        return;
    }

    INTERRUPT_SCOPE();

    cpu_t* self = cpu_get();

    list_entry_init(&rcu->entry);
    rcu->func = func;
    rcu->arg = arg;

    list_push_back(self->rcu.batch, &rcu->entry);
}

void rcu_report_quiescent(cpu_t* self)
{
    lock_acquire(&lock);
    if (active && bitmap_is_set(&ack, self->id))
    {
        bitmap_clear(&ack, self->id);

        if (bitmap_is_empty(&ack))
        {
            active = false;
        }
    }

    bool wake = false;
    if (!list_is_empty(self->rcu.waiting))
    {
        if (grace > self->rcu.grace || (grace == self->rcu.grace && !active))
        {
            wake = true;
        }
    }
    lock_release(&lock);

    while (!list_is_empty(self->rcu.ready))
    {
        list_entry_t* entry = list_pop_front(self->rcu.ready);
        rcu_entry_t* rcu = CONTAINER_OF(entry, rcu_entry_t, entry);
        rcu->func(rcu->arg);
    }

    if (wake)
    {
        list_t* temp = self->rcu.ready;
        self->rcu.ready = self->rcu.waiting;
        self->rcu.waiting = temp;
    }

    if (list_is_empty(self->rcu.waiting) && !list_is_empty(self->rcu.batch))
    {
        list_t* temp = self->rcu.waiting;
        self->rcu.waiting = self->rcu.batch;
        self->rcu.batch = temp;

        lock_acquire(&lock);
        self->rcu.grace = grace + 1;
        lock_release(&lock);
    }

    if (list_is_empty(self->rcu.waiting))
    {
        return;
    }

    LOCK_SCOPE(&lock);
    if (active)
    {
        return;
    }

    active = true;
    grace++;

    bitmap_set_range(&ack, 0, cpu_amount());
    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
        if (!sched_is_idle(cpu))
        {
            continue;
        }

        ipi_wake_up(cpu, IPI_SINGLE);
    }
}

void rcu_call_free(void* arg)
{
    free(arg);
}