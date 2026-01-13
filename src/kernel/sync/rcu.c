#include <kernel/cpu/cpu.h>
#include <kernel/cpu/ipi.h>
#include <kernel/log/log.h>
#include <kernel/mem/cache.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rcu.h>

#include <stdlib.h>
#include <sys/bitmap.h>

static BITMAP_CREATE_ZERO(ack, CPU_MAX);
static uint64_t grace = 0;
static bool active = false;
static lock_t lock = LOCK_CREATE();

typedef struct rcu
{
    uint64_t grace;  ///< The last grace period observed by this CPU.
    list_t* batch;   ///< Callbacks queued during the current grace period.
    list_t* waiting; ///< Callbacks waiting for the current grace period to end.
    list_t* ready;   ///< Callbacks whose grace period has ended.
    list_t lists[3]; //< Buffer storing three lists such that we can rotate them.
} rcu_t;

PERCPU_DEFINE_CTOR(static rcu_t, pcpu_rcu)
{
    rcu_t* rcu = SELF_PTR(pcpu_rcu);

    rcu->grace = 0;
    rcu->batch = &rcu->lists[0];
    rcu->waiting = &rcu->lists[1];
    rcu->ready = &rcu->lists[2];
    for (size_t i = 0; i < ARRAY_SIZE(rcu->lists); i++)
    {
        list_init(&rcu->lists[i]);
    }
}

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

void rcu_call(rcu_entry_t* entry, rcu_callback_t func, void* arg)
{
    if (entry == NULL || func == NULL)
    {
        return;
    }

    CLI_SCOPE();

    list_entry_init(&entry->entry);
    entry->func = func;
    entry->arg = arg;

    list_push_back(pcpu_rcu->batch, &entry->entry);
}

void rcu_report_quiescent(void)
{
    lock_acquire(&lock);
    if (active && bitmap_is_set(&ack, SELF->id))
    {
        bitmap_clear(&ack, SELF->id);

        if (bitmap_is_empty(&ack))
        {
            active = false;
        }
    }

    bool wake = false;
    if (!list_is_empty(pcpu_rcu->waiting))
    {
        if (grace > pcpu_rcu->grace || (grace == pcpu_rcu->grace && !active))
        {
            wake = true;
        }
    }
    lock_release(&lock);

    while (!list_is_empty(pcpu_rcu->ready))
    {
        rcu_entry_t* entry = CONTAINER_OF(list_pop_front(pcpu_rcu->ready), rcu_entry_t, entry);
        entry->func(entry->arg);
    }

    if (wake)
    {
        list_t* temp = pcpu_rcu->ready;
        pcpu_rcu->ready = pcpu_rcu->waiting;
        pcpu_rcu->waiting = temp;
    }

    if (list_is_empty(pcpu_rcu->waiting) && !list_is_empty(pcpu_rcu->batch))
    {
        list_t* temp = pcpu_rcu->waiting;
        pcpu_rcu->waiting = pcpu_rcu->batch;
        pcpu_rcu->batch = temp;

        lock_acquire(&lock);
        pcpu_rcu->grace = grace + 1;
        lock_release(&lock);
    }

    if (list_is_empty(pcpu_rcu->waiting))
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

void rcu_call_cache_free(void* arg)
{
    cache_free(arg);
}