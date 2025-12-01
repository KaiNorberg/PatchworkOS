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
#include <stdint.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>
#include <time.h>

static wait_queue_t sleepQueue = WAIT_QUEUE_CREATE(sleepQueue);

// Adjusts the given vclock value by the given lag using long division to maintain precision. Used to implement eq 18
// and 19.
static inline void vclock_adjust_by_lag(vclock_t* vclock, vclock_t* remainder, lag_t lag, int64_t totalWeight,
    bool subtract)
{
    assert(vclock != NULL);
    assert(remainder != NULL);
    assert(totalWeight > 0);

    vclock_t vdelta = lag / totalWeight;
    vclock_t vdeltaRemainder = lag % totalWeight;

    if (vdeltaRemainder < 0)
    {
        vdelta -= 1;
        vdeltaRemainder += totalWeight;
    }

    assert(vdeltaRemainder >= 0 && vdeltaRemainder < totalWeight);

    if (subtract)
    {
        *vclock -= vdelta;
        if (*remainder < vdeltaRemainder)
        {
            *vclock -= 1;
            *remainder += totalWeight;
        }
        *remainder -= vdeltaRemainder;
    }
    else
    {
        *vclock += vdelta;
        *remainder += vdeltaRemainder;
    }

    if (*remainder >= totalWeight)
    {
        *vclock += *remainder / totalWeight;
        *remainder %= totalWeight;
    }
    else if (*remainder < 0)
    {
        *vclock -= 1;
        *remainder += totalWeight;
    }

    assert(*remainder >= 0 && *remainder < totalWeight);
}

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

static void sched_client_update(rbnode_t* node)
{
    sched_client_t* client = CONTAINER_OF(node, sched_client_t, node);

    vclock_t minEligible = client->veligible;
    for (rbnode_direction_t dir = RBNODE_LEFT; dir <= RBNODE_RIGHT; dir++)
    {
        rbnode_t* child = node->children[dir];
        if (child == NULL)
        {
            continue;
        }

        sched_client_t* childClient = CONTAINER_OF(child, sched_client_t, node);
        minEligible = MIN(minEligible, childClient->vminEligibleTime);
    }
    client->vminEligibleTime = minEligible;
}

static void sched_vtime_update(sched_t* sched, clock_t uptime)
{
    assert(sched != NULL);

    clock_t delta = uptime - sched->lastUpdate;
    sched->lastUpdate = uptime;

    if (sched->totalWeight == 0)
    {
        sched->vtime = 0;
        sched->vtimeRemainder = 0;
        return;
    }

    assert(sched->vtimeRemainder >= 0 && sched->vtimeRemainder < sched->totalWeight);

    // Eq 5.
    vclock_t vdeltaNumerator = (vclock_t)delta + sched->vtimeRemainder;
    vclock_t vdelta = vdeltaNumerator / sched->totalWeight;
    sched->vtime += vdelta;
    sched->vtimeRemainder = vdeltaNumerator % sched->totalWeight;

    if (sched->runThread != sched->idleThread)
    {
        // Eq 3 and 6.
        lag_t idealService = (lag_t)vdelta * sched->runThread->sched.weight;
        sched->runThread->sched.lag += idealService - (lag_t)delta;
    }
}

void sched_client_init(sched_client_t* client)
{
    assert(client != NULL);

    client->node = RBNODE_CREATE;
    client->weight = UINT32_MAX; // Invalid
    client->vdeadline = VCLOCKS_NEVER;
    client->veligible = VCLOCKS_NEVER;
    client->vminEligibleTime = VCLOCKS_NEVER;
    client->start = 0;
    client->vleave = VCLOCKS_NEVER;
    client->timeSliceEnd = 0;
    client->lag = 0;
}

void sched_init(sched_t* sched)
{
    assert(sched != NULL);

    sched->totalWeight = 0;
    rbtree_init(&sched->runqueue, sched_client_compare, sched_client_update);
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

    bootThread->sched.weight = atomic_load(&bootThread->process->priority) + CONFIG_WEIGHT_BASE;
    bootThread->sched.start = sys_time_uptime();
    bootThread->sched.timeSliceEnd = bootThread->sched.start + CONFIG_TIME_SLICE;
    bootThread->sched.veligible = sched->vtime;
    bootThread->sched.vdeadline = bootThread->sched.veligible + (vclock_t)CONFIG_TIME_SLICE / bootThread->sched.weight;
    atomic_store(&bootThread->state, THREAD_ACTIVE);

    sched->totalWeight += bootThread->sched.weight;
    sched->runThread = bootThread;
    rbtree_insert(&sched->runqueue, &bootThread->sched.node);

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

void sched_enter(thread_t* thread, cpu_t* target)
{
    assert(thread != NULL);

    cpu_t* self = cpu_get();
    if (target == NULL)
    {
        target = self;
    }

    assert(thread != target->sched.idleThread);

    sched_t* sched = &target->sched;
    lock_acquire(&sched->lock);

    clock_t uptime = sys_time_uptime();
    sched_vtime_update(sched, uptime);

    sched_client_t* client = &thread->sched;
    client->weight = atomic_load(&thread->process->priority) + CONFIG_WEIGHT_BASE;
    sched->totalWeight += client->weight;

    if (client->vleave != VCLOCKS_NEVER)
    {
        lag_t waitTime = (lag_t)(sched->vtime - client->vleave) * client->weight;
        client->lag += waitTime;
        client->vleave = VCLOCKS_NEVER;
    }

    lag_t lag = client->lag; // Maintain conservation?
    client->lag = CLAMP(client->lag, -(lag_t)CONFIG_MAX_LAG, (lag_t)CONFIG_MAX_LAG);

    vclock_adjust_by_lag(&sched->vtime, &sched->vtimeRemainder, lag, sched->totalWeight, true);

    client->veligible = sched->vtime;
    client->vdeadline = client->veligible + (vclock_t)CONFIG_TIME_SLICE / client->weight;

    rbtree_insert(&sched->runqueue, &client->node);
    atomic_store(&thread->state, THREAD_ACTIVE);

    bool shouldWake = self != target || !self->interrupt.inInterrupt;
    lock_release(&sched->lock);
    cpu_put();

    if (shouldWake)
    {
        ipi_wake_up(target, IPI_SINGLE);
    }
}

static void sched_leave(sched_t* sched, thread_t* thread)
{
    assert(sched != NULL);
    assert(thread != NULL);
    assert(thread != sched->idleThread);

    sched_client_t* client = &thread->sched;

    client->vleave = sched->vtime;
    sched->totalWeight -= client->weight;

    if (sched->totalWeight == 0)
    {
        sched->vtime = 0;
        sched->vtimeRemainder = 0;
        rbtree_remove(&sched->runqueue, &client->node);
        return;
    }

    vclock_adjust_by_lag(&sched->vtime, &sched->vtimeRemainder, client->lag, sched->totalWeight, false);

    rbtree_remove(&sched->runqueue, &client->node);
}

// Should be called with sched lock held.
static thread_t* sched_first_eligible(sched_t* sched)
{
    assert(sched != NULL);

    rbnode_t* current = sched->runqueue.root;
    while (current != NULL)
    {
        sched_client_t* client = CONTAINER_OF(current, sched_client_t, node);

        rbnode_t* left = current->children[RBNODE_LEFT];
        if (left != NULL)
        {
            sched_client_t* leftClient = CONTAINER_OF(left, sched_client_t, node);
            if (leftClient->vminEligibleTime <= sched->vtime)
            {
                current = left;
                continue;
            }
        }

        if (client->veligible <= sched->vtime)
        {
            return CONTAINER_OF(client, thread_t, sched);
        }

        current = current->children[RBNODE_RIGHT];
    }

    if (!rbtree_is_empty(&sched->runqueue))
    {
        // Jump forward to the next eligible time, we need this as rounding errors will inevitably
        // cause us to miss eligibles otherwise.
        sched_client_t* root = CONTAINER_OF(sched->runqueue.root, sched_client_t, node);
        LOG_DEBUG("no eligible threads, jumping vtime from %lld to %lld\n", sched->vtime, root->vminEligibleTime);
        sched->vtime = root->vminEligibleTime;
        return sched_first_eligible(sched);
    }

    return sched->idleThread;
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
        thread_t* thread = CONTAINER_OF(client, thread_t, sched);
        sched_leave(&self->sched, thread);
        lock_release(&self->sched.lock);

        sched_enter(thread, neighbor);
        ipi_wake_up(neighbor, IPI_SINGLE);
    }
    else
    {
        lock_release(&self->sched.lock);
    }
}

#ifndef NDEBUG
static void sched_verify_min_eligible(sched_t* sched, rbnode_t* node)
{
    sched_client_t* client = CONTAINER_OF(node, sched_client_t, node);
    for (rbnode_direction_t dir = RBNODE_LEFT; dir <= RBNODE_RIGHT; dir++)
    {
        rbnode_t* child = node->children[dir];
        if (child == NULL)
        {
            continue;
        }

        sched_client_t* childClient = CONTAINER_OF(child, sched_client_t, node);
        if (client->vminEligibleTime > childClient->vminEligibleTime)
        {
            panic(NULL, "vminEligibleTime incorrect for node with vdeadline %lld, expected %lld but got %lld",
                client->vdeadline, childClient->vminEligibleTime, client->vminEligibleTime);
        }

        sched_verify_min_eligible(sched, child);
    }
}

static void sched_verify(sched_t* sched)
{
    int64_t totalWeight = 0;
    sched_client_t* client;
    RBTREE_FOR_EACH(client, &sched->runqueue, node)
    {
        totalWeight += client->weight;
        assert(client->weight > 0);
    }

    if (totalWeight != sched->totalWeight)
    {
        panic(NULL, "sched totalWeight incorrect, expected %lld but got %lld", totalWeight, sched->totalWeight);
    }

    assert(sched->vtimeRemainder >= 0 && sched->vtimeRemainder < MAX(sched->totalWeight, 1));

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

    sched_client_t* root = CONTAINER_OF_SAFE(sched->runqueue.root, sched_client_t, node);
    if (root != NULL)
    {
        sched_verify_min_eligible(sched, &root->node);
    }
}
#endif

void sched_do(interrupt_frame_t* frame, cpu_t* self)
{
    assert(frame != NULL);
    assert(self != NULL);

    //sched_load_balance(self);

    sched_t* sched = &self->sched;
    lock_acquire(&sched->lock);

    if (sched->runThread == sched->idleThread && rbtree_is_empty(&sched->runqueue)) // Nothing to do
    {
        lock_release(&sched->lock);
        return;
    }

    // Prevents the compiler from being annoying.
    thread_t* volatile runThread = sched->runThread;
    assert(runThread != NULL);
    assert(sched->idleThread != NULL);

    // Cant free any potential threads while still using its address space, so we defer the free to after we have
    // switched threads. Since we have a per-CPU interrupt stack, we dont need to worry about a use-after-free of the
    // stack.
    thread_t* volatile threadToFree = NULL;

#ifndef NDEBUG
    sched_verify(sched);
#endif

    clock_t uptime = sys_time_uptime();
    sched_vtime_update(sched, uptime);

    thread_state_t state = atomic_load(&runThread->state);
    switch (state)
    {
    case THREAD_DYING:
        assert(runThread != sched->idleThread);

        threadToFree = runThread;
        sched_leave(sched, runThread);
        break;
    case THREAD_PRE_BLOCK:
    case THREAD_UNBLOCKING:
        assert(runThread != sched->idleThread);
        if (wait_block_finalize(frame, self, runThread, uptime))
        {
            sched_leave(sched, runThread);
            break;
        }

        // Early unblock
        atomic_store(&runThread->state, THREAD_ACTIVE);
        // Fallthrough
    case THREAD_ACTIVE:
        if (runThread != sched->idleThread)
        {
            clock_t used = uptime - runThread->sched.start;
            runThread->sched.start = uptime;

            // Eq 12, advance virtual eligible time by the amount of virtual time the thread has used.
            runThread->sched.veligible += (vclock_t)used / runThread->sched.weight;
            // Eq 10, set new virtual deadline.
            runThread->sched.vdeadline = runThread->sched.veligible + (vclock_t)CONFIG_TIME_SLICE / runThread->sched.weight;

            rbtree_fix(&sched->runqueue, &runThread->sched.node);
        }
        break;
    default:
        panic(NULL, "Thread in invalid state in sched_do() state=%d", state);
    }

    thread_t* next = sched_first_eligible(sched);
    assert(next != NULL);

    if (next != runThread)
    {
        thread_save(runThread, frame);

        next->sched.start = uptime;
        next->sched.timeSliceEnd = uptime + CONFIG_TIME_SLICE;

        assert(atomic_load(&next->state) == THREAD_ACTIVE);
        thread_load(next, frame);
        runThread = next;
    }
    else if (runThread != sched->idleThread && uptime >= runThread->sched.timeSliceEnd)
    {
        runThread->sched.timeSliceEnd = uptime + CONFIG_TIME_SLICE;
    }

    if (runThread != sched->idleThread)
    {
        timer_set(uptime, runThread->sched.timeSliceEnd);
    }

    if (threadToFree != NULL)
    {
        assert(threadToFree != runThread);
        thread_free(threadToFree);
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
