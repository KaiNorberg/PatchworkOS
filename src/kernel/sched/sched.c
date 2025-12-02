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
        minEligible = MIN(minEligible, childClient->vminEligible);
    }
    client->vminEligible = minEligible;
}

static void sched_vtime_reset(sched_t* sched, clock_t uptime)
{
    assert(sched != NULL);

    sched->lastUpdate = uptime;
    sched->vtime = 0;
    sched->resetCounter++;
}

static void sched_vtime_update(sched_t* sched, clock_t uptime)
{
    assert(sched != NULL);

    clock_t delta = uptime - sched->lastUpdate;
    sched->lastUpdate = uptime;

    if (sched->totalWeight == 0)
    {
        sched_vtime_reset(sched, uptime);
        return;
    }

    // Eq 5.
    sched->vtime += (vclock_t)delta / sched->totalWeight;
}

void sched_client_init(sched_client_t* client)
{
    assert(client != NULL);

    client->node = RBNODE_CREATE;
    client->weight = UINT32_MAX; // Invalid
    client->vdeadline = VCLOCKS_NEVER;
    client->veligible = VCLOCKS_NEVER;
    client->vminEligible = VCLOCKS_NEVER;
    client->start = 0;
    client->vleave = VCLOCKS_NEVER;
    client->resetCounter = 0;
}

void sched_init(sched_t* sched)
{
    assert(sched != NULL);

    sched->totalWeight = 0;
    rbtree_init(&sched->runqueue, sched_client_compare, sched_client_update);
    sched->vtime = 0;
    sched->lastUpdate = 0;
    sched->resetCounter = 0;
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
    assert(rbtree_is_empty(&sched->runqueue));
    assert(sched->totalWeight == 0);

    bootThread->sched.weight = atomic_load(&bootThread->process->priority) + CONFIG_WEIGHT_BASE;
    bootThread->sched.start = sys_time_uptime();
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

    // TODO: 

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

    if (client->vleave != VCLOCKS_NEVER && sched->resetCounter == client->resetCounter)
    {
        lag_t lag = client->weight * (client->vleave - client->veligible);
        lag = CLAMP(lag, -((lag_t)CONFIG_MAX_LAG), (lag_t)CONFIG_MAX_LAG);
        sched->vtime -= lag / sched->totalWeight;
    }
    client->vleave = VCLOCKS_NEVER;

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

static void sched_leave(sched_t* sched, thread_t* thread, clock_t uptime)
{
    assert(sched != NULL);
    assert(thread != NULL);
    assert(thread != sched->idleThread);

    sched_client_t* client = &thread->sched;

    lag_t lag = client->weight * (sched->vtime - client->veligible);
    client->vleave = sched->vtime;
    client->resetCounter = sched->resetCounter;
    sched->totalWeight -= client->weight;

    rbtree_remove(&sched->runqueue, &client->node);

    if (sched->totalWeight == 0)
    {
        sched_vtime_reset(sched, uptime);
        return;
    }

    sched->vtime += lag / sched->totalWeight;
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
            if (leftClient->vminEligible <= sched->vtime)
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
        LOG_DEBUG("no eligible threads, diff=%lld\n", sched->vtime - root->vminEligible);
        sched->vtime = root->vminEligible;
        return sched_first_eligible(sched);
    }

    return sched->idleThread;
}

static void sched_acquire_two(sched_t* a, sched_t* b)
{
    if (a < b)
    {
        lock_acquire(&a->lock);
        lock_acquire(&b->lock);
    }
    else if (a > b)
    {
        lock_acquire(&b->lock);
        lock_acquire(&a->lock);
    }
    else
    {
        lock_acquire(&a->lock);
    }
}

static void sched_release_two(sched_t* a, sched_t* b)
{
    if (a != b)
    {
        lock_release(&a->lock);
        lock_release(&b->lock);
    }
    else
    {
        lock_release(&a->lock);
    }
}

static void sched_load_balance(cpu_t* self)
{
    assert(self != NULL);

    cpu_t* neighbor = cpu_get_next(self);
    if (neighbor == self)
    {
        return;
    }

    sched_acquire_two(&self->sched, &neighbor->sched);

    uint64_t selfLoad = self->sched.runqueue.size;
    uint64_t neighborLoad = neighbor->sched.runqueue.size;
    if (neighborLoad + CONFIG_LOAD_BALANCE_BIAS >= selfLoad)
    {
        sched_release_two(&self->sched, &neighbor->sched);
        return;
    }

    sched_client_t* client = CONTAINER_OF_SAFE(rbtree_find_max(self->sched.runqueue.root), sched_client_t, node);
    if (client != NULL)
    {
        if (client == &self->sched.runThread->sched)
        {
            client = CONTAINER_OF_SAFE(
                rbtree_prev(&client->node), sched_client_t, node);
            if (client == NULL)
            {
                sched_release_two(&self->sched, &neighbor->sched);
                return;
            }
        }
        thread_t* thread = CONTAINER_OF(client, thread_t, sched);

        self->sched.totalWeight -= client->weight;
        rbtree_remove(&self->sched.runqueue, &client->node);

        client->veligible = neighbor->sched.vtime;
        client->vdeadline = client->veligible + (vclock_t)CONFIG_TIME_SLICE / client->weight;

        neighbor->sched.totalWeight += client->weight;
        rbtree_insert(&neighbor->sched.runqueue, &client->node);

        ipi_wake_up(neighbor, IPI_SINGLE);
    }

    sched_release_two(&self->sched, &neighbor->sched);
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
        if (client->vminEligible > childClient->vminEligible)
        {
            panic(NULL, "vminEligible incorrect for node with vdeadline %lld, expected %lld but got %lld",
                client->vdeadline, childClient->vminEligible, client->vminEligible);
        }

        sched_verify_min_eligible(sched, child);
    }
}

static void sched_verify(sched_t* sched)
{
    uint64_t size = 0;
    int64_t totalWeight = 0;
    sched_client_t* client;
    RBTREE_FOR_EACH(client, &sched->runqueue, node)
    {
        totalWeight += client->weight;
        assert(client->weight > 0);
        size++;
    }

    if (size != sched->runqueue.size)
    {
        panic(NULL, "sched runqueue size incorrect, expected %llu but got %llu", size, sched->runqueue.size);
    }

    if (totalWeight != sched->totalWeight)
    {
        panic(NULL, "sched totalWeight incorrect, expected %lld but got %lld", totalWeight, sched->totalWeight);
    }

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

    /*LOG_DEBUG("debug info:\n");
    sched_client_t* iter;
    lag_t sumLag = 0;
    RBTREE_FOR_EACH(iter, &sched->runqueue, node)
    {
        thread_t* thread = CONTAINER_OF(iter, thread_t, sched);
        lag_t lag = iter->weight * (sched->vtime - iter->veligible);
        LOG_DEBUG("  process %lld thread %lld lag=%lld vtime=%llu veligible=%llu weight=%lld\n", 
            thread->process->id, thread->id, lag, sched->vtime, iter->veligible, iter->weight);
        sumLag += lag;
    }
    LOG_DEBUG("  sum lag=%lld\n", sumLag);
    LOG_DEBUG("\n");*/
}
#endif

void sched_do(interrupt_frame_t* frame, cpu_t* self)
{
    assert(frame != NULL);
    assert(self != NULL);

    sched_load_balance(self);

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

    clock_t uptime = sys_time_uptime();
    sched_vtime_update(sched, uptime);

    if (runThread != sched->idleThread)
    {
        clock_t used = uptime - runThread->sched.start;
        runThread->sched.start = uptime;

        // Eq 12, advance virtual eligible time by the amount of virtual time the thread has used.
        runThread->sched.veligible += (vclock_t)used / runThread->sched.weight;
        // Eq 10, set new virtual deadline.
        runThread->sched.vdeadline =
            runThread->sched.veligible + (vclock_t)CONFIG_TIME_SLICE / runThread->sched.weight;

        rbtree_fix(&sched->runqueue, &runThread->sched.node);
    }

#ifndef NDEBUG
    sched_verify(sched);
#endif

    thread_state_t state = atomic_load(&runThread->state);
    switch (state)
    {
    case THREAD_DYING:
        assert(runThread != sched->idleThread);

        threadToFree = runThread;
        sched_leave(sched, runThread, uptime);
        break;
    case THREAD_PRE_BLOCK:
    case THREAD_UNBLOCKING:
        assert(runThread != sched->idleThread);
        if (wait_block_finalize(frame, self, runThread, uptime))
        {
            sched_leave(sched, runThread, uptime);
            break;
        }

        // Early unblock
        atomic_store(&runThread->state, THREAD_ACTIVE);
        // Fallthrough
    case THREAD_ACTIVE:
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

        assert(atomic_load(&next->state) == THREAD_ACTIVE);
        thread_load(next, frame);
        runThread = next;
    }

    if (runThread != sched->idleThread)
    {
        vclock_t vtimeout = MAX(runThread->sched.vdeadline - sched->vtime, 0);
        timer_set(uptime, uptime + (vtimeout * sched->totalWeight));
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
