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

static int64_t sched_ctx_compare(const rbnode_t* aNode, const rbnode_t* bNode)
{
    const sched_ctx_t* a = CONTAINER_OF(aNode, sched_ctx_t, node);
    const sched_ctx_t* b = CONTAINER_OF(bNode, sched_ctx_t, node);

    int64_t diff = (int64_t)a->vdeadline - (int64_t)b->vdeadline;
    if (diff != 0)
    {
        return diff;
    }

    // If they are tied, then the one that has run less in virtual time is favored
    diff = (int64_t)a->vruntime - (int64_t)b->vruntime;
    if (diff != 0)
    {
        return diff;
    }

    return 0;
}

static inline vclock_t sched_clock_to_vclock(clock_t clock, uint64_t weight)
{
    assert(weight != 0);
    return (clock * CONFIG_WEIGHT_BASE) / weight;
}

static void sched_update_min_vruntime(sched_t* sched)
{
    assert(sched != NULL);

    sched_ctx_t* min = CONTAINER_OF_SAFE(rbtree_find_min(sched->runqueue.root), sched_ctx_t, node);
    if (min == NULL)
    {
        return;
    }

    sched->minVruntime = MAX(sched->minVruntime, min->vruntime);
}

static void sched_update_vdeadline(sched_t* sched, sched_ctx_t* ctx)
{
    assert(sched != NULL);
    assert(ctx != NULL);

    if (ctx->vruntime < sched->minVruntime)
    {
        ctx->vruntime = sched->minVruntime;
    }

    ctx->timeSlice = CONFIG_TIME_SLICE;
    vclock_t vtimeSlice = sched_clock_to_vclock(ctx->timeSlice, ctx->weight);
    ctx->vdeadline = ctx->vruntime + vtimeSlice;
}

static void sched_insert(sched_t* sched, sched_ctx_t* ctx)
{
    assert(sched != NULL);
    assert(ctx != NULL);

    sched_update_vdeadline(sched, ctx);
    rbtree_insert(&sched->runqueue, &ctx->node);
    sched->totalWeight += ctx->weight;
}

void sched_ctx_init(sched_ctx_t* ctx)
{
    assert(ctx != NULL);

    assert(ctx != NULL);
    ctx->node = RBNODE_CREATE;
    ctx->weight = CONFIG_WEIGHT_BASE;
    ctx->vruntime = 0;
    ctx->vdeadline = 0;
    ctx->lastUpdate = 0;
    ctx->timeSlice = CONFIG_TIME_SLICE;
}

void sched_init(sched_t* sched)
{
    assert(sched != NULL);

    rbtree_init(&sched->runqueue, sched_ctx_compare);
    sched->minVruntime = 0;
    sched->totalWeight = 0;
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
    sched_t* schedCtx = &self->sched;

    lock_acquire(&schedCtx->lock);

    assert(self->sched.runThread == self->sched.idleThread);

    sched_ctx_t* ctx = &bootThread->sched;
    ctx->weight = atomic_load(&bootThread->process->priority) + CONFIG_WEIGHT_BASE;
    ctx->lastUpdate = sys_time_uptime();
    ctx->vruntime = 0;
    ctx->vdeadline = ctx->vruntime + sched_clock_to_vclock(CONFIG_TIME_SLICE, ctx->weight);

    schedCtx->runThread = bootThread;
    atomic_store(&bootThread->state, THREAD_ACTIVE);

    lock_release(&schedCtx->lock);
    thread_jump(bootThread);
}

uint64_t sched_nanosleep(clock_t timeout)
{
    return WAIT_BLOCK_TIMEOUT(&sleepQueue, false, timeout);
}

bool sched_is_idle(cpu_t* cpu)
{
    assert(cpu != NULL);

    sched_t* schedCtx = &cpu->sched;
    LOCK_SCOPE(&schedCtx->lock);

    return schedCtx->runThread == schedCtx->idleThread;
}

thread_t* sched_thread(void)
{
    cpu_t* self = cpu_get();
    sched_t* schedCtx = &self->sched;
    thread_t* thread = schedCtx->runThread;
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
    sched_t* schedCtx = &self->sched;
    return schedCtx->runThread;
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

    sched_ctx_t* ctx = &thread->sched;
    ctx->weight = atomic_load(&thread->process->priority) + CONFIG_WEIGHT_BASE;
    sched_insert(sched, ctx);
    atomic_store(&thread->state, THREAD_ACTIVE);

    bool shouldWake = self != target || !self->interrupt.inInterrupt;

    lock_release(&sched->lock);
    cpu_put();

    if (shouldWake)
    {
        ipi_wake_up(target, IPI_SINGLE);
    }
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

    sched_ctx_t* ctx = CONTAINER_OF_SAFE(rbtree_find_max(self->sched.runqueue.root), sched_ctx_t, node);
    if (ctx != NULL)
    {
        rbtree_remove(&self->sched.runqueue, &ctx->node);
        self->sched.totalWeight -= ctx->weight;

        lock_release(&self->sched.lock);

        thread_t* thread = CONTAINER_OF(ctx, thread_t, sched);
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

    assert(sched->runThread->frame.rflags & RFLAGS_INTERRUPT_ENABLE);
    // Prevent the compiler from being annoying.
    thread_t* volatile runThread = sched->runThread;
    assert(runThread != NULL);

    clock_t uptime = sys_time_uptime();
    if (runThread != sched->idleThread)
    {
        clock_t delta = uptime - runThread->sched.lastUpdate;
        vclock_t vdelta = sched_clock_to_vclock(delta, runThread->sched.weight);

        runThread->sched.vruntime += vdelta;
        runThread->sched.lastUpdate = uptime;

        if (delta >= runThread->sched.timeSlice)
        {
            runThread->sched.timeSlice = 0;
        }
        else
        {
            runThread->sched.timeSlice -= delta;
        }
    }

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
            runThread = NULL;
        }
        else // Early unblock
        {
            atomic_store(&runThread->state, THREAD_ACTIVE);
        }
    }
    break;
    case THREAD_ACTIVE:
    {
        if (runThread == sched->idleThread)
        {
            if (!rbtree_is_empty(&sched->runqueue))
            {
                runThread = NULL;
            }
            break;
        }

        if (runThread->sched.timeSlice == 0)
        {
            thread_save(runThread, frame);
            sched_insert(sched, &runThread->sched);
            runThread = NULL;
        }
    }
    break;
    case THREAD_DYING:
    {
        assert(runThread != sched->idleThread);

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
        sched_update_min_vruntime(sched);

        sched_ctx_t* nextCtx = CONTAINER_OF_SAFE(rbtree_find_min(sched->runqueue.root), sched_ctx_t, node);
        if (nextCtx != NULL)
        {
            rbtree_remove(&sched->runqueue, &nextCtx->node);
            sched->totalWeight -= nextCtx->weight;

            runThread = CONTAINER_OF(nextCtx, thread_t, sched);
            runThread->sched.lastUpdate = uptime;
            timer_set(uptime, uptime + runThread->sched.timeSlice);
        }
        else
        {
            runThread = sched->idleThread;
        }

        atomic_store(&runThread->state, THREAD_ACTIVE);
        thread_load(runThread, frame);
    }
    else
    {
        runThread->sched.lastUpdate = uptime;
    }

    if (threadToFree != NULL)
    {
        thread_free(threadToFree);
    }

    sched->runThread = runThread;
    assert(sched->runThread->frame.rflags & RFLAGS_INTERRUPT_ENABLE);
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
    cpu_t* self = cpu_get();
    thread_t* thread = self->sched.runThread;

    if (thread != self->sched.idleThread)
    {
        thread->sched.timeSlice = 0;
    }

    cpu_put();
    ipi_invoke();
    return 0;
}
