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
#include <kernel/sched/clock.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/utils/rbtree.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/bitmap.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>
#include <time.h>

static wait_queue_t sleepQueue = WAIT_QUEUE_CREATE(sleepQueue);

static _Atomic(clock_t) lastLoadBalance = ATOMIC_VAR_INIT(0);

static inline int64_t sched_fixed_cmp(int128_t a, int128_t b)
{
    int128_t diff = SCHED_FIXED_FROM(a - b);
    if (diff > SCHED_EPSILON)
    {
        return 1;
    }

    if (diff < -(SCHED_EPSILON + 1))
    {
        return -1;
    }

    return 0;
}

static int64_t sched_node_compare(const rbnode_t* aNode, const rbnode_t* bNode)
{
    const sched_client_t* a = CONTAINER_OF(aNode, sched_client_t, node);
    const sched_client_t* b = CONTAINER_OF(bNode, sched_client_t, node);

    if (a->vdeadline < b->vdeadline)
    {
        return -1;
    }

    if (a->vdeadline > b->vdeadline)
    {
        return 1;
    }

    if (a < b)
    {
        return -1;
    }

    if (a > b)
    {
        return 1;
    }

    return 0;
}

static void sched_node_update(rbnode_t* node)
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
        if (sched_fixed_cmp(childClient->vminEligible, minEligible) < 0)
        {
            minEligible = childClient->vminEligible;
        }
    }
    client->vminEligible = minEligible;
}

PERCPU_DEFINE_CTOR(sched_t, _pcpu_sched)
{
    sched_t* sched = SELF_PTR(_pcpu_sched);

    atomic_init(&sched->totalWeight, 0);
    rbtree_init(&sched->runqueue, sched_node_compare, sched_node_update);
    sched->vtime = SCHED_FIXED_ZERO;
    sched->lastUpdate = 0;
    lock_init(&sched->lock);
    atomic_init(&sched->preemptCount, 0);

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

static bool sched_is_cache_hot(thread_t* thread, clock_t uptime)
{
    return thread->sched.stop + CONFIG_CACHE_HOT_THRESHOLD > uptime;
}

void sched_client_init(sched_client_t* client)
{
    assert(client != NULL);

    client->node = RBNODE_CREATE;
    client->weight = UINT32_MAX; // Invalid
    client->vdeadline = SCHED_FIXED_ZERO;
    client->veligible = SCHED_FIXED_ZERO;
    client->vminEligible = SCHED_FIXED_ZERO;
    client->stop = 0;
    client->lastCpu = NULL;
}

void sched_client_update_veligible(sched_client_t* client, vclock_t newVeligible)
{
    assert(client != NULL);

    client->veligible = newVeligible;
    client->vminEligible = newVeligible;
    client->vdeadline = newVeligible + SCHED_FIXED_TO(CONFIG_TIME_SLICE) / client->weight;
}

// Must be called with the scheduler lock held.
static void sched_vtime_reset(sched_t* sched, clock_t uptime)
{
    assert(sched != NULL);

    sched->lastUpdate = uptime;
    sched->vtime = SCHED_FIXED_ZERO;
}

// Must be called with the scheduler lock held.
static void sched_vtime_update(sched_t* sched, clock_t uptime)
{
    assert(sched != NULL);

    clock_t delta = uptime - sched->lastUpdate;
    sched->lastUpdate = uptime;

    int64_t totalWeight = atomic_load(&sched->totalWeight);
    if (totalWeight == 0 || sched->runThread == sched->idleThread)
    {
        sched_vtime_reset(sched, uptime);
        return;
    }

    // Eq 5.
    sched->vtime += SCHED_FIXED_TO(delta) / totalWeight;

    sched_client_update_veligible(&sched->runThread->sched,
        sched->runThread->sched.veligible + SCHED_FIXED_TO(delta) / sched->runThread->sched.weight);
    rbtree_fix(&sched->runqueue, &sched->runThread->sched.node);
}

// Should be called with sched lock held.
static void sched_enter(sched_t* sched, thread_t* thread, clock_t uptime)
{
    assert(sched != NULL);
    assert(thread != NULL);
    assert(thread != sched->idleThread);

    sched_vtime_update(sched, uptime);

    sched_client_t* client = &thread->sched;
    client->weight = atomic_load(&thread->process->priority) + SCHED_WEIGHT_BASE;
    atomic_fetch_add(&sched->totalWeight, client->weight);

    sched_client_update_veligible(client, sched->vtime);

    rbtree_insert(&sched->runqueue, &client->node);
    atomic_store(&thread->state, THREAD_ACTIVE);
}

// Should be called with sched lock held.
static void sched_leave(sched_t* sched, thread_t* thread, clock_t uptime)
{
    assert(sched != NULL);
    assert(thread != NULL);
    assert(thread != sched->idleThread);

    sched_client_t* client = &thread->sched;

    lag_t lag = (sched->vtime - client->veligible) * client->weight;
    int64_t totalWeight = atomic_fetch_sub(&sched->totalWeight, client->weight) - client->weight;

    rbtree_remove(&sched->runqueue, &client->node);

    if (totalWeight == 0)
    {
        sched_vtime_reset(sched, uptime);
        return;
    }

    // Adjust the scheduler's time such that the sum of all threads' lag remains zero.
    sched->vtime += lag / totalWeight;
}

static cpu_t* sched_get_least_loaded(void)
{
    cpu_t* leastLoaded = NULL;
    uint64_t leastLoad = UINT64_MAX;

    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
        sched_t* sched = CPU_PTR(cpu->id, _pcpu_sched);

        uint64_t load = atomic_load(&sched->totalWeight);

        if (load < leastLoad)
        {
            leastLoad = load;
            leastLoaded = cpu;
        }
    }

    assert(leastLoaded != NULL);
    return leastLoaded;
}

static thread_t* sched_steal(void)
{
    sched_t* mostLoaded = NULL;
    uint64_t mostLoad = 0;

    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
        if (cpu->id == SELF->id)
        {
            continue;
        }

        sched_t* sched = CPU_PTR(cpu->id, _pcpu_sched);
        uint64_t load = atomic_load(&sched->totalWeight);

        if (load > mostLoad)
        {
            mostLoad = load;
            mostLoaded = sched;
        }
    }

    if (mostLoaded == NULL)
    {
        return NULL;
    }

    lock_acquire(&mostLoaded->lock);

    clock_t uptime = clock_uptime();

    thread_t* thread;
    RBTREE_FOR_EACH(thread, &mostLoaded->runqueue, sched.node)
    {
        if (thread == mostLoaded->runThread || sched_is_cache_hot(thread, uptime))
        {
            continue;
        }

        sched_leave(mostLoaded, thread, uptime);
        lock_release(&mostLoaded->lock);
        return thread;
    }

    lock_release(&mostLoaded->lock);
    return NULL;
}

// Should be called with scheduler lock held.
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
            if (sched_fixed_cmp(leftClient->vminEligible, sched->vtime) <= 0)
            {
                current = left;
                continue;
            }
        }

        if (sched_fixed_cmp(client->veligible, sched->vtime) <= 0)
        {
            return CONTAINER_OF(client, thread_t, sched);
        }

        current = current->children[RBNODE_RIGHT];
    }

#ifndef NDEBUG
    if (!rbtree_is_empty(&sched->runqueue))
    {
        vclock_t vminEligible =
            CONTAINER_OF_SAFE(rbtree_find_min(sched->runqueue.root), sched_client_t, node)->vminEligible;
        panic(NULL, "No eligible threads found, vminEligible=%lld vtime=%lld", SCHED_FIXED_FROM(vminEligible),
            SCHED_FIXED_FROM(sched->vtime));
    }
#endif

    thread_t* victim = sched_steal();
    if (victim != NULL)
    {
        sched_enter(sched, victim, clock_uptime());
        return victim;
    }

    return sched->idleThread;
}

void sched_start(thread_t* bootThread)
{
    assert(bootThread != NULL);

    sched_t* self = SELF_PTR(_pcpu_sched);

    lock_acquire(&self->lock);

    assert(self->runThread == self->idleThread);
    assert(rbtree_is_empty(&self->runqueue));
    assert(atomic_load(&self->totalWeight) == 0);

    bootThread->sched.weight = atomic_load(&bootThread->process->priority) + SCHED_WEIGHT_BASE;
    sched_client_update_veligible(&bootThread->sched, self->vtime);
    atomic_store(&bootThread->state, THREAD_ACTIVE);

    atomic_fetch_add(&self->totalWeight, bootThread->sched.weight);
    self->runThread = bootThread;
    rbtree_insert(&self->runqueue, &bootThread->sched.node);

    lock_release(&self->lock);
    thread_jump(bootThread);
}

void sched_submit(thread_t* thread)
{
    assert(thread != NULL);

    cli_push();
    sched_t* self = SELF_PTR(_pcpu_sched);

    cpu_t* target;
    if (thread->sched.lastCpu != NULL && sched_is_cache_hot(thread, clock_uptime()))
    {
        target = thread->sched.lastCpu;
    }
    else if (atomic_load(&self->totalWeight) == 0)
    {
        target = SELF->self;
    }
    else
    {
        target = sched_get_least_loaded();
    }

    sched_t* sched = CPU_PTR(target->id, _pcpu_sched);

    lock_acquire(&sched->lock);
    sched_enter(sched, thread, clock_uptime());
    lock_release(&sched->lock);

    bool shouldWake = SELF->id != target->id || !SELF->inInterrupt;
    cli_pop();

    if (shouldWake)
    {
        ipi_wake_up(target, IPI_SINGLE);
    }
}

#ifndef NDEBUG
static void sched_verify_min_eligible(sched_t* sched, rbnode_t* node)
{
    sched_client_t* client = CONTAINER_OF(node, sched_client_t, node);

    bool hasChildren = false;
    for (rbnode_direction_t dir = RBNODE_LEFT; dir <= RBNODE_RIGHT; dir++)
    {
        rbnode_t* child = node->children[dir];
        if (child == NULL)
        {
            continue;
        }

        hasChildren = true;

        sched_client_t* childClient = CONTAINER_OF(child, sched_client_t, node);

        if (sched_fixed_cmp(client->vminEligible, childClient->vminEligible) > 0)
        {
            panic(NULL, "vminEligible incorrect for node with vdeadline %lld, expected %lld but got %lld",
                SCHED_FIXED_FROM(client->vdeadline), SCHED_FIXED_FROM(childClient->vminEligible),
                SCHED_FIXED_FROM(client->vminEligible));
        }

        sched_verify_min_eligible(sched, child);
    }

    if (!hasChildren && sched_fixed_cmp(client->vminEligible, client->veligible) != 0)
    {
        panic(NULL, "Leaf node vminEligible != veligible, vminEligible=%lld veligible=%lld",
            SCHED_FIXED_FROM(client->vminEligible), SCHED_FIXED_FROM(client->veligible));
    }
}

static void sched_verify(sched_t* sched)
{
    int64_t totalWeight = 0;
    bool runThreadFound = false;
    sched_client_t* client;
    RBTREE_FOR_EACH(client, &sched->runqueue, node)
    {
        thread_t* thread = CONTAINER_OF(client, thread_t, sched);
        totalWeight += client->weight;
        assert(client->weight > 0);
        assert(thread != sched->idleThread);

        if (atomic_load(&thread->state) != THREAD_ACTIVE && thread != sched->runThread)
        {
            panic(NULL, "Thread in runqueue has invalid state %d", atomic_load(&thread->state));
        }

        if (thread == sched->runThread)
        {
            runThreadFound = true;
        }
    }

    if (sched->runThread != sched->idleThread && !runThreadFound)
    {
        panic(NULL, "Running thread not found in runqueue");
    }

    if (totalWeight != atomic_load(&sched->totalWeight))
    {
        panic(NULL, "sched totalWeight incorrect, expected %lld but got %lld", totalWeight,
            atomic_load(&sched->totalWeight));
    }

    sched_client_t* min = CONTAINER_OF_SAFE(rbtree_find_min(sched->runqueue.root), sched_client_t, node);
    if (min != NULL)
    {
        sched_client_t* other;
        RBTREE_FOR_EACH(other, &sched->runqueue, node)
        {
            if (sched_fixed_cmp(other->vdeadline, min->vdeadline) < 0)
            {
                panic(NULL, "runqueue not sorted, node with vdeadline %lld found, but min is %lld",
                    SCHED_FIXED_FROM(other->vdeadline), SCHED_FIXED_FROM(min->vdeadline));
            }
        }
    }

    sched_client_t* root = CONTAINER_OF_SAFE(sched->runqueue.root, sched_client_t, node);
    if (root != NULL)
    {
        sched_verify_min_eligible(sched, &root->node);
    }

    sched_client_t* iter;
    lag_t sumLag = SCHED_FIXED_ZERO;
    RBTREE_FOR_EACH(iter, &sched->runqueue, node)
    {
        lag_t lag = (sched->vtime - iter->veligible) * iter->weight;
        sumLag += lag;
    }
    if (sched_fixed_cmp(sumLag, SCHED_FIXED_ZERO) != 0)
    {
        LOG_DEBUG("debug info (vtime=%lld lagValue=%lld lagFixed=%lld):\n", SCHED_FIXED_FROM(sched->vtime),
            SCHED_FIXED_FROM(sumLag), sumLag);
        RBTREE_FOR_EACH(iter, &sched->runqueue, node)
        {
            thread_t* thread = CONTAINER_OF(iter, thread_t, sched);
            lag_t lag = (sched->vtime - iter->veligible) * iter->weight;
            LOG_DEBUG("  process %lld thread %lld lag=%lld veligible=%lld vdeadline=%lld weight=%lld\n",
                thread->process->id, thread->id, SCHED_FIXED_FROM(lag), SCHED_FIXED_FROM(iter->veligible),
                SCHED_FIXED_FROM(iter->vdeadline), iter->weight);
        }
        panic(NULL, "Total lag is not zero, got %lld", SCHED_FIXED_FROM(sumLag));
    }
}
#endif

void sched_do(interrupt_frame_t* frame)
{
    assert(frame != NULL);

    sched_t* sched = SELF_PTR(_pcpu_sched);
    lock_acquire(&sched->lock);

    if (atomic_load(&sched->preemptCount) > 0 && atomic_load(&sched->runThread->state) == THREAD_ACTIVE)
    {
        lock_release(&sched->lock);
        return;
    }

    if (sched->runThread == sched->idleThread && rbtree_is_empty(&sched->runqueue)) // Nothing to do
    {
        lock_release(&sched->lock);
        rcu_report_quiescent();
        return;
    }

    clock_t uptime = clock_uptime();
    sched_vtime_update(sched, uptime);

    assert(sched->runThread != NULL);
    assert(sched->idleThread != NULL);

    // Cant free any potential threads while still using its address space, so we defer the free to after we have
    // switched threads. Since we have a per-CPU interrupt stack, we dont need to worry about a use-after-free of the
    // stack.
    thread_t* volatile threadToFree = NULL;

#ifndef NDEBUG
    sched_verify(sched);
#endif

    thread_state_t state = atomic_load(&sched->runThread->state);
    switch (state)
    {
    case THREAD_DYING:
        assert(sched->runThread != sched->idleThread);

        threadToFree = sched->runThread;
        sched_leave(sched, sched->runThread, uptime);
        break;
    case THREAD_PRE_BLOCK:
    case THREAD_UNBLOCKING:
        assert(sched->runThread != sched->idleThread);
        if (wait_block_finalize(frame, sched->runThread, uptime))
        {
            sched_leave(sched, sched->runThread, uptime);
            break;
        }

        // Early unblock
        atomic_store(&sched->runThread->state, THREAD_ACTIVE);
        break;
    case THREAD_ACTIVE:
        break;
    default:
        panic(NULL, "Thread in invalid state in sched_do() state=%d", state);
    }

    thread_t* next = sched_first_eligible(sched);
    assert(next != NULL);

    if (next != sched->runThread)
    {
        sched->runThread->sched.lastCpu = SELF->self;
        sched->runThread->sched.stop = uptime;
        thread_save(sched->runThread, frame);
        assert(atomic_load(&next->state) == THREAD_ACTIVE);
        thread_load(next, frame);
        sched->runThread = next;
    }

    lock_release(&sched->lock);

    if (sched->runThread != sched->idleThread)
    {
        timer_set(uptime, uptime + CONFIG_TIME_SLICE);
    }

    if (threadToFree != NULL)
    {
        assert(threadToFree != sched->runThread);
        thread_free(threadToFree); // Cant hold any locks here
    }

    rcu_report_quiescent();
}

bool sched_is_idle(cpu_t* cpu)
{
    assert(cpu != NULL);

    sched_t* sched = CPU_PTR(cpu->id, _pcpu_sched);

    LOCK_SCOPE(&sched->lock);
    return sched->runThread == sched->idleThread;
}

uint64_t sched_nanosleep(clock_t timeout)
{
    return WAIT_BLOCK_TIMEOUT(&sleepQueue, false, timeout);
}

void sched_yield(void)
{
    sched_nanosleep(CLOCKS_PER_MS);
}

void sched_disable(void)
{
    CLI_SCOPE();

    sched_t* sched = SELF_PTR(_pcpu_sched);
    atomic_fetch_add(&sched->preemptCount, 1);
}

void sched_enable(void)
{
    CLI_SCOPE();

    sched_t* sched = SELF_PTR(_pcpu_sched);
    atomic_fetch_sub(&sched->preemptCount, 1);
}

void sched_process_exit(const char* status)
{
    thread_t* thread = thread_current();
    process_kill(thread->process, status);

    atomic_store(&thread->state, THREAD_DYING);
    ipi_invoke();
    panic(NULL, "Return to sched_process_exit");
}

void sched_thread_exit(void)
{
    thread_t* thread = thread_current();
    atomic_store(&thread->state, THREAD_DYING);
    ipi_invoke();
    panic(NULL, "Return to sched_thread_exit");
}

SYSCALL_DEFINE(SYS_NANOSLEEP, uint64_t, clock_t nanoseconds)
{
    return sched_nanosleep(nanoseconds);
}

SYSCALL_DEFINE(SYS_PROCESS_EXIT, void, const char* status)
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
