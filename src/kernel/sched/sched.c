#include <_internal/config.h>
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
#include <kernel/sched/sched.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>

#include <assert.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>
#include <time.h>

static wait_queue_t sleepQueue = WAIT_QUEUE_CREATE(sleepQueue);

static int64_t sched_thread_ctx_compare(sched_thread_ctx_t* a, sched_thread_ctx_t* b)
{
    if (a->virtualDeadline < b->virtualDeadline)
    {
        return -1;
    }
    else if (a->virtualDeadline > b->virtualDeadline)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void sched_thread_ctx_init(sched_thread_ctx_t* ctx)
{
    assert(ctx != NULL);

    ctx->node = RBNODE_CREATE;
    ctx->eligibleTime = 0;
    ctx->virtualDeadline = 0;
    ctx->virtualRuntime = 0;
    ctx->virtualStartTime = 0;
    ctx->startTime = 0;
    ctx->timesScheduled = 0;
    ctx->weight = 0;
}

void sched_cpu_ctx_init(sched_cpu_ctx_t* ctx)
{
    assert(ctx != NULL);

    rbtree_init(&ctx->runqueue, (rbnode_compare_t)sched_thread_ctx_compare);
    ctx->totalWeight = 0;
    lock_init(&ctx->lock);

    ctx->idleThread = thread_new(process_get_kernel());
    if (ctx->idleThread == NULL)
    {
        panic(NULL, "Failed to create idle thread");
    }

    ctx->idleThread->frame.rip = (uintptr_t)sched_idle_loop;
    ctx->idleThread->frame.rsp = ctx->idleThread->kernelStack.top;
    ctx->idleThread->frame.cs = GDT_CS_RING0;
    ctx->idleThread->frame.ss = GDT_SS_RING0;
    ctx->idleThread->frame.rflags = RFLAGS_INTERRUPT_ENABLE | RFLAGS_ALWAYS_SET;

    ctx->runThread = ctx->idleThread;
    atomic_store(&ctx->runThread->state, THREAD_RUNNING);
}

static void sched_update_virtual_deadline(sched_cpu_ctx_t* sched, sched_thread_ctx_t* thread, clock_t uptime)
{
    assert(sched != NULL);
    assert(thread != NULL);

    if (thread->startTime == 0)
    {
        thread->startTime = uptime;
    }

    clock_t expectedRuntime = ((uptime - thread->startTime) * sched->totalWeight) / thread->weight;

    if (expectedRuntime > thread->virtualRuntime)
    {
        clock_t lag = expectedRuntime - thread->virtualRuntime;
        thread->eligibleTime = uptime + lag;
    }
    else
    {
        thread->eligibleTime = uptime;
    }

    thread->virtualDeadline =
        thread->eligibleTime + CONFIG_TIME_SLICE * (PRIORITY_MAX / (PRIORITY_MAX - thread->weight + 1));
}

void sched_start(thread_t* bootThread)
{
    assert(bootThread != NULL);

    cpu_t* self = cpu_get_unsafe();
    sched_cpu_ctx_t* schedCtx = &self->sched;

    lock_acquire(&schedCtx->lock);

    assert(self->sched.runThread == self->sched.idleThread);
    atomic_store(&self->sched.idleThread->state, THREAD_READY);

    sched_thread_ctx_t* threadCtx = &bootThread->sched;
    threadCtx->virtualDeadline = sys_time_uptime();
    threadCtx->virtualRuntime = 0;
    threadCtx->timesScheduled = 1;
    threadCtx->weight = atomic_load(&bootThread->process->priority);

    schedCtx->runThread = bootThread;
    atomic_store(&bootThread->state, THREAD_RUNNING);

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

    sched_cpu_ctx_t* schedCtx = &cpu->sched;
    LOCK_SCOPE(&schedCtx->lock);

    return schedCtx->runThread == schedCtx->idleThread;
}

thread_t* sched_thread(void)
{
    cpu_t* self = cpu_get();
    sched_cpu_ctx_t* schedCtx = &self->sched;
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
    sched_cpu_ctx_t* schedCtx = &self->sched;
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

void sched_push(thread_t* thread, cpu_t* target)
{
    assert(thread != NULL);

    cpu_t* self = cpu_get();
    if (target == NULL)
    {
        target = self;
    }

    sched_cpu_ctx_t* sched = &target->sched;
    lock_acquire(&sched->lock);

    clock_t uptime = sys_time_uptime();

    sched_thread_ctx_t* threadCtx = &thread->sched;
    threadCtx->weight = atomic_load(&thread->process->priority);
    sched_update_virtual_deadline(sched, threadCtx, uptime);
    atomic_store(&thread->state, THREAD_READY);

    rbtree_insert(&sched->runqueue, &threadCtx->node);
    sched->totalWeight += threadCtx->weight;

    bool shouldWake = self != target || !self->interrupt.inInterrupt;

    lock_release(&sched->lock);
    cpu_put();

    if (shouldWake)
    {
        ipi_wake_up(target, IPI_SINGLE);
    }
}

void sched_do(interrupt_frame_t* frame, cpu_t* self)
{
    assert(frame != NULL);
    assert(self != NULL);

    sched_cpu_ctx_t* sched = &self->sched;
    lock_acquire(&sched->lock);

    clock_t uptime = sys_time_uptime();

    thread_t* volatile runThread = sched->runThread; // Prevent the compiler from being annoying.
    if (runThread == NULL)
    {
        lock_release(&sched->lock);
        panic(NULL, "No running thread in sched_do()");
    }

    if (runThread->sched.virtualStartTime == 0)
    {
        runThread->sched.virtualStartTime = uptime;
    }
    runThread->sched.virtualRuntime +=
        ((uptime - runThread->sched.virtualStartTime) * sched->totalWeight) / runThread->sched.weight;
    runThread->sched.virtualStartTime = uptime;

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

        if (wait_block_finalize(frame, self, runThread, uptime)) // Block finalized
        {
            thread_save(runThread, frame);
            runThread = NULL; // Force a new thread to be loaded
        }
        else // Early unblock
        {
            atomic_store(&runThread->state, THREAD_RUNNING);
        }
    }
    break;
    case THREAD_RUNNING:
    {
        // Do nothing
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
        if (rbtree_is_empty(&sched->runqueue))
        {
            runThread = sched->idleThread;
        }
        else
        {
            rbnode_t* node = rbtree_find_min(sched->runqueue.root);
            rbtree_remove(&sched->runqueue, node);

            runThread = CONTAINER_OF(node, thread_t, sched.node);
            sched->totalWeight -= runThread->sched.weight;

            thread_state_t oldState = atomic_exchange(&runThread->state, THREAD_RUNNING);
            assert(oldState == THREAD_READY);
            runThread->sched.timesScheduled++;
            thread_load(runThread, frame);
        }

        atomic_store(&runThread->state, THREAD_RUNNING);
    }
    else if (uptime >= runThread->sched.virtualDeadline)
    {
        thread_t* next = CONTAINER_OF_SAFE(rbtree_find_min(sched->runqueue.root), thread_t, sched.node);
        if (next != NULL)
        {
            sched_update_virtual_deadline(sched, &runThread->sched, uptime);

            thread_state_t oldState = atomic_exchange(&runThread->state, THREAD_READY);
            assert(oldState == THREAD_RUNNING);
            thread_save(runThread, frame);

            rbtree_insert(&sched->runqueue, &runThread->sched.node);
            sched->totalWeight += runThread->sched.weight;

            rbtree_remove(&sched->runqueue, &next->sched.node);
            sched->totalWeight -= next->sched.weight;
            thread_state_t nextOldState = atomic_exchange(&next->state, THREAD_RUNNING);
            assert(nextOldState == THREAD_READY);
            next->sched.timesScheduled++;
            thread_load(next, frame);

            runThread = next;
        }
    }

    if (threadToFree != NULL)
    {
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
    thread_t* thread = cpu_get()->sched.runThread;
    thread->sched.virtualDeadline = 0;
    cpu_put();

    ipi_invoke();
    return 0;
}
