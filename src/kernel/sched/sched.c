#include <_internal/clock_t.h>
#include <kernel/config.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/irq.h>
#include <kernel/sched/sched.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/syscall.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/proc/process.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <kernel/utils/rbtree.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>
#include <time.h>

static wait_queue_t sleepQueue = WAIT_QUEUE_CREATE(sleepQueue);

static int64_t sched_client_compare(const rbnode_t* aNode, const rbnode_t* bNode)
{
    const sched_client_t* a = CONTAINER_OF(aNode, sched_client_t, node);
    const sched_client_t* b = CONTAINER_OF(bNode, sched_client_t, node);

    if (a->vdeadline < b->vdeadline)
    {
        return -1;
    }
    else if (a->vdeadline > b->vdeadline)
    {
        return 1;
    }

    // Use the address as a tie breaker just to ensure a consistent ordering.
    if (a < b)
    {
        return -1;
    }
    else if (a > b)
    {
        return 1;
    }

    return 0;
}

static void sched_vtime_update(sched_t* sched, clock_t uptime)
{
    assert(sched != NULL);

    clock_t delta = uptime - sched->lastUpdate;
    sched->lastUpdate = uptime;

    if (sched->totalWeight == 0)
    {
        sched->vtimeRemainder = 0;
        return;
    }
    sched->runThread->sched.lag -= (lag_t)delta;

    delta += sched->vtimeRemainder;
    vclock_t vdelta = (vclock_t)delta / sched->totalWeight;

    sched->vtimeRemainder = (vclock_t)delta % sched->totalWeight;
    sched->vtime += vdelta;
}

static sched_client_t* sched_runqueue_first(sched_t* sched)
{
    assert(sched != NULL);

#ifdef DEBUG
    sched_client_t* min = CONTAINER_OF_SAFE(rbtree_find_min(sched->runqueue.root), sched_client_t, node);
    if (min != NULL)
    {
        sched_client_t* other;
        RBTREE_FOR_EACH(other, &sched->runqueue, node)
        {
            if (other->vdeadline < min->vdeadline)
            {
                panic(NULL, "runqueue not sorted, node with vdeadline %lld found, but min is %lld", other->vdeadline,
                    min->vdeadline);
            }
        }
    }
#endif

    sched_client_t* client;
    RBTREE_FOR_EACH(client, &sched->runqueue, node)
    {
        if (client->veligibleTime <= sched->vtime)
        {
            return client;
        }
    }

    return CONTAINER_OF_SAFE(rbtree_find_min(sched->runqueue.root), sched_client_t, node);
}

void sched_client_init(sched_client_t* client)
{
    assert(client != NULL);

    client->node = RBNODE_CREATE;
    client->weight = CONFIG_WEIGHT_BASE;
    client->vdeadline = CLOCKS_NEVER;
    client->veligibleTime = 0;
    client->leaveTime = CLOCKS_NEVER;
    client->timeSliceStart = 0;
    client->timeSliceEnd = 0;
    client->lag = 0;
}

void sched_init(sched_t* sched)
{
    assert(sched != NULL);

    sched->totalWeight = 0;
    rbtree_init(&sched->runqueue, sched_client_compare);
    sched->vtimeRemainder = 0;
    sched->vtime = 0;
    sched->lastUpdate = 0;
    lock_init(&sched->lock);

    sched->idleThread = thread_new(process_get_kernel());
    if (sched->idleThread == NULL)
    {
        panic(NULL, "Failed to create idle thread");
    }

    sched->idleThread->frame.rip = (uintptr_t)sched_idle_loop;
    sched->idleThread->frame.rsp = sched->idleThread->kernelStack.top;
    sched->idleThread->frame.cs = GDT_CS_RING0;
    sched->idleThread->frame.ss = GDT_SS_RING0;
    sched->idleThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    sched->runThread = sched->idleThread;
    atomic_store(&sched->runThread->state, THREAD_ACTIVE);
}

void sched_start(thread_t* bootThread)
{
    assert(bootThread != NULL);

    cpu_t* self = cpu_get_unsafe();
    sched_t* sched = &self->sched;

    lock_acquire(&sched->lock);

    assert(self->sched.runThread == self->sched.idleThread);

    bootThread->sched.timeSliceStart = sys_time_uptime();
    bootThread->sched.timeSliceEnd = bootThread->sched.timeSliceStart + CONFIG_TIME_SLICE;
    bootThread->sched.veligibleTime = sched->vtime;
    bootThread->sched.vdeadline =
        bootThread->sched.veligibleTime + (vclock_t)CONFIG_TIME_SLICE / bootThread->sched.weight;

    sched->totalWeight += bootThread->sched.weight;

    sched->runThread = bootThread;
    atomic_store(&bootThread->state, THREAD_ACTIVE);

    lock_release(&sched->lock);
    thread_jump(bootThread);
}

uint64_t sched_nanosleep(clock_t timeout)
{
    return WAIT_BLOCK_TIMEOUT(&sleepQueue, false, timeout);
}

bool sched_is_idle(cpu_t* cpu)
{
    assert(cpu != NULL);

    sched_t* schedclient = &cpu->sched;
    LOCK_SCOPE(&schedclient->lock);

    return schedclient->runThread == schedclient->idleThread;
}

thread_t* sched_thread(void)
{
    cpu_t* self = cpu_get();
    sched_t* schedclient = &self->sched;
    thread_t* thread = schedclient->runThread;
    cpu_put();
    return thread;
}

process_t* sched_process(void)
{
    thread_t* thread = sched_thread();
    return thread->process;
}

thread_t* sched_thread_unsafe(void)
{
    cpu_t* self = cpu_get_unsafe();
    sched_t* schedclient = &self->sched;
    return schedclient->runThread;
}

process_t* sched_process_unsafe(void)
{
    thread_t* thread = sched_thread_unsafe();
    return thread->process;
}

void sched_process_exit(uint64_t status)
{
    thread_t* thread = sched_thread();
    process_kill(thread->process, status);
    ipi_invoke();

    panic(NULL, "sched_process_exit() returned unexpectedly");
}

void sched_thread_exit(void)
{
    thread_t* thread = sched_thread();
    thread_kill(thread);
    ipi_invoke();

    panic(NULL, "Return to sched_thread_exit");
}

void sched_yield(void)
{
    cpu_t* self = cpu_get();
    thread_t* thread = self->sched.runThread;
    thread->sched.timeSliceEnd = 0;
    cpu_put();
    ipi_invoke();
}

void sched_submit(thread_t* thread, cpu_t* target)
{
    assert(thread != NULL);

    cpu_t* self = cpu_get();
    if (target == NULL)
    {
        target = self;
    }

    sched_t* sched = &target->sched;
    lock_acquire(&sched->lock);

    clock_t uptime = sys_time_uptime();
    sched_vtime_update(sched, uptime);

    sched_client_t* client = &thread->sched;
    client->weight = atomic_load(&thread->process->priority) + CONFIG_WEIGHT_BASE;
    sched->totalWeight += client->weight;

    if (client->leaveTime != CLOCKS_NEVER)
    {
        clock_t delta = uptime - client->leaveTime;
        client->lag += (vclock_t)delta * client->weight;
        client->leaveTime = CLOCKS_NEVER;
    }

    if (client->lag >= (lag_t)CONFIG_MAX_LAG)
    {
        client->lag = CONFIG_MAX_LAG;
    }

    sched->vtime -= client->lag / sched->totalWeight;

    client->veligibleTime = sched->vtime;
    client->vdeadline = client->veligibleTime + (vclock_t)CONFIG_TIME_SLICE / client->weight;
    rbtree_insert(&sched->runqueue, &client->node);

    bool shouldWake = self != target || !self->interrupt.inInterrupt;
    lock_release(&sched->lock);
    cpu_put();

    if (shouldWake)
    {
        ipi_wake_up(target, IPI_SINGLE);
    }
}

static void sched_leave(sched_t* sched, thread_t* thread, clock_t uptime)
{
    assert(sched != NULL);
    assert(thread != NULL);
    assert(thread != sched->idleThread);

    sched_client_t* client = &thread->sched;

    sched->totalWeight -= client->weight;
    client->leaveTime = uptime;

    if (sched->totalWeight == 0)
    {
        sched->vtime = 0;
        sched->vtimeRemainder = 0;
        return;
    }

    sched->vtime += client->lag / sched->totalWeight;
}

static uint64_t sched_get_load(sched_t* sched)
{
    LOCK_SCOPE(&sched->lock);
    return sched->runqueue.size + (sched->runThread != sched->idleThread ? 1 : 0);
}

static void sched_load_balance(cpu_t* self)
{
    assert(self != NULL);

    // Technically there are race conditions here, but the worst case scenario is imperfect load balancing
    // and so its an acceptable trade off since we need to avoid holding the locks of two sched_t at the
    // same time to prevent deadlocks.

    cpu_t* neighbor = cpu_get_next(self);
    if (neighbor == self)
    {
        return;
    }

    uint64_t selfLoad = sched_get_load(&self->sched);
    uint64_t neighborLoad = sched_get_load(&neighbor->sched);
    if (neighborLoad + CONFIG_LOAD_BALANCE_BIAS >= selfLoad)
    {
        return;
    }

    lock_acquire(&self->sched.lock);

    sched_client_t* client = CONTAINER_OF_SAFE(rbtree_find_max(self->sched.runqueue.root), sched_client_t, node);
    if (client != NULL)
    {
        rbtree_remove(&self->sched.runqueue, &client->node);
        thread_t* thread = CONTAINER_OF(client, thread_t, sched);
        sched_leave(&self->sched, thread, sys_time_uptime());
        lock_release(&self->sched.lock);

        sched_submit(thread, neighbor);
        ipi_wake_up(neighbor, IPI_SINGLE);
    }
    else
    {
        lock_release(&self->sched.lock);
    }
}

void sched_do(interrupt_frame_t* frame, cpu_t* self)
{
    assert(frame != NULL);
    assert(self != NULL);

    sched_load_balance(self);

    sched_t* sched = &self->sched;
    lock_acquire(&sched->lock);

    // Prevent the compiler from being annoying.
    thread_t* volatile runThread = sched->runThread;
    assert(runThread != NULL);

    clock_t uptime = sys_time_uptime();
    sched_vtime_update(sched, uptime);

    // Cant free the thread while still using its address space, so we defer the free to after we have switched threads.
    thread_t* volatile threadToFree = NULL;
    thread_state_t state = atomic_load(&runThread->state);
    switch (state)
    {
    case THREAD_PRE_BLOCK:
    case THREAD_UNBLOCKING:
    {
        assert(runThread != sched->idleThread);
        thread_save(runThread, frame);

        if (wait_block_finalize(frame, self, runThread, uptime))
        {
            thread_save(runThread, frame);
            sched_leave(sched, runThread, uptime);
            runThread = NULL;
            break;
        }

        // Early unblock
        atomic_store(&runThread->state, THREAD_ACTIVE);
    }
    case THREAD_ACTIVE: // Fallthrough
    {
        if (runThread == sched->idleThread)
        {
            if (!rbtree_is_empty(&sched->runqueue))
            {
                runThread = NULL;
            }
            break;
        }

        if (runThread->sched.timeSliceEnd <= uptime && !rbtree_is_empty(&sched->runqueue))
        {
            thread_save(runThread, frame);

            clock_t used = uptime - runThread->sched.timeSliceStart;
            runThread->sched.veligibleTime += (vclock_t)used / runThread->sched.weight;
            runThread->sched.vdeadline =
                runThread->sched.veligibleTime + (vclock_t)CONFIG_TIME_SLICE / runThread->sched.weight;
            rbtree_insert(&sched->runqueue, &runThread->sched.node);
            runThread = NULL;
        }
    }
    break;
    case THREAD_DYING:
    {
        assert(runThread != sched->idleThread);

        sched_leave(sched, runThread, uptime);
        threadToFree = runThread;
        runThread = NULL;
    }
    break;
    default:
    {
        panic(NULL, "Invalid thread state %d (pid=%d tid=%d)", state, runThread->process->id, runThread->id);
    }
    }

    if (runThread == NULL)
    {
        sched_client_t* next = sched_runqueue_first(sched);
        if (next != NULL)
        {
            rbtree_remove(&sched->runqueue, &next->node);
            runThread = CONTAINER_OF(next, thread_t, sched);

            runThread->sched.timeSliceStart = uptime;
            runThread->sched.timeSliceEnd = uptime + CONFIG_TIME_SLICE;
        }
        else
        {
            runThread = sched->idleThread;
        }

        atomic_store(&runThread->state, THREAD_ACTIVE);
        thread_load(runThread, frame);
    }

    if (threadToFree != NULL)
    {
        thread_free(threadToFree);
    }

    if (runThread != sched->idleThread)
    {
        if (runThread->sched.timeSliceEnd <= uptime) // Give another slice
        {
            runThread->sched.timeSliceEnd = uptime + CONFIG_TIME_SLICE;
        }

        timer_set(uptime, runThread->sched.timeSliceEnd);
    }

    sched->runThread = runThread;
    lock_release(&sched->lock);
}

SYSCALL_DEFINE(SYS_NANOSLEEP, uint64_t, clock_t nanoseconds)
{
    return sched_nanosleep(nanoseconds);
}

SYSCALL_DEFINE(SYS_PROCESS_EXIT, void, uint64_t status)
{
    sched_process_exit(status);

    panic(NULL, "Return to syscall_process_exit");
}

SYSCALL_DEFINE(SYS_THREAD_EXIT, void)
{
    sched_thread_exit();

    panic(NULL, "Return to syscall_thread_exit");
}

SYSCALL_DEFINE(SYS_YIELD, uint64_t)
{
    sched_yield();
    return 0;
}
