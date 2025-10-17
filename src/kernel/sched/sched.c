#include "sched.h"

#include "cpu/gdt.h"
#include "cpu/interrupt.h"
#include "cpu/smp.h"
#include "cpu/syscalls.h"
#include "drivers/apic.h"
#include "log/log.h"
#include "log/panic.h"
#include "proc/process.h"
#include "sched/sched.h"
#include "sched/thread.h"
#include "sched/timer.h"
#include "sched/wait.h"
#include "sync/lock.h"

#include <assert.h>
#include <sys/list.h>
#include <sys/math.h>
#include <sys/proc.h>

static wait_queue_t sleepQueue;

static inline void sched_queues_init(sched_queues_t* queues)
{
    queues->length = 0;
    queues->bitmap = 0;
    for (uint64_t i = PRIORITY_MIN; i < PRIORITY_MAX; i++)
    {
        list_init(&queues->lists[i]);
    }
}

static inline void sched_queues_push(sched_queues_t* queues, thread_t* thread)
{
    assert(thread->sched.actualPriority < PRIORITY_MAX);

    queues->length++;
    queues->bitmap |= (1ULL << thread->sched.actualPriority);
    list_push(&queues->lists[thread->sched.actualPriority], &thread->entry);
}

static inline thread_t* sched_queues_pop(sched_queues_t* queues, priority_t minPriority)
{
    if (queues->length == 0 || queues->bitmap == 0)
    {
        assert(queues->length == 0 && queues->bitmap == 0);
        return NULL;
    }

    priority_t highestPriority = PRIORITY_MAX - 1 - __builtin_clzll(queues->bitmap);
    if (minPriority > highestPriority)
    {
        return NULL;
    }

    queues->length--;
    thread_t* thread = CONTAINER_OF(list_pop(&queues->lists[highestPriority]), thread_t, entry);

    if (list_is_empty(&queues->lists[highestPriority]))
    {
        queues->bitmap &= ~(1ULL << highestPriority);
    }

    return thread;
}

void sched_thread_ctx_init(sched_thread_ctx_t* ctx)
{
    ctx->timeSlice = 0;
    ctx->deadline = 0;
    ctx->actualPriority = 0;
    ctx->recentBlockTime = 0;
    ctx->prevBlockCheck = 0;
}

void sched_cpu_ctx_init(sched_cpu_ctx_t* ctx, cpu_t* cpu)
{
    sched_queues_init(&ctx->queues[0]);
    sched_queues_init(&ctx->queues[1]);
    ctx->active = &ctx->queues[0];
    ctx->expired = &ctx->queues[1];
    list_init(&ctx->zombieThreads);

    // Bootstrap cpu is initalized early so we cant yet create the idle thread, the boot thread on the bootstrap cpu
    // will become the idle thread.
    if (cpu->id == CPU_BOOTSTRAP_ID)
    {
        ctx->idleThread = NULL;
        ctx->runThread = NULL;
    }
    else
    {
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

    lock_init(&ctx->lock);
}

static void sched_init_spawn_boot_thread(void)
{
    cpu_t* self = smp_self_unsafe();
    assert(self->sched.runThread == NULL);

    thread_t* bootThread = thread_get_boot();
    assert(bootThread != NULL);

    bootThread->sched.deadline = UINT64_MAX;
    atomic_store(&bootThread->state, THREAD_RUNNING);
    self->sched.runThread = bootThread;
}

void sched_init(void)
{
    sched_init_spawn_boot_thread();

    wait_queue_init(&sleepQueue);
}

void sched_done_with_boot_thread(void)
{
    cpu_t* self = smp_self_unsafe();
    sched_cpu_ctx_t* ctx = &self->sched;

    assert(self->id == CPU_BOOTSTRAP_ID && ctx->runThread->process == process_get_kernel() && ctx->runThread->id == 0);

    // The boot thread becomes the bootstrap cpus idle thread.
    ctx->runThread->sched.deadline = 0;
    ctx->idleThread = ctx->runThread;

    asm volatile("sti");

    timer_notify_self();
    // When we return here the boot thread will be an idle thread so we just enter the idle loop.
    sched_idle_loop();
}

wait_result_t sched_nanosleep(clock_t timeout)
{
    return WAIT_BLOCK_TIMEOUT(&sleepQueue, false, timeout);
}

SYSCALL_DEFINE(SYS_NANOSLEEP, uint64_t, clock_t nanoseconds)
{
    return sched_nanosleep(nanoseconds);
}

bool sched_is_idle(void)
{
    sched_cpu_ctx_t* ctx = &smp_self()->sched;
    bool isIdle = ctx->runThread == ctx->idleThread;
    smp_put();
    return isIdle;
}

thread_t* sched_thread(void)
{
    thread_t* thread = smp_self()->sched.runThread;
    smp_put();
    return thread;
}

process_t* sched_process(void)
{
    thread_t* thread = sched_thread();
    if (thread == NULL)
    {
        return NULL;
    }

    return thread->process;
}

void sched_process_exit(uint64_t status)
{
    thread_t* thread = sched_thread();
    process_t* process = sched_process();

    thread_kill(thread);
    process_kill(process, status);
}

SYSCALL_DEFINE(SYS_PROCESS_EXIT, void, uint64_t status)
{
    sched_process_exit(status);
    timer_notify_self();
    panic(NULL, "Return to syscall_process_exit");
}

void sched_thread_exit(void)
{
    thread_kill(sched_thread());
}

SYSCALL_DEFINE(SYS_THREAD_EXIT, void)
{
    sched_thread_exit();
    timer_notify_self();
    panic(NULL, "Return to syscall_thread_exit");
}

static void sched_update_recent_idle_time(thread_t* thread, bool wasBlocking)
{
    clock_t uptime = timer_uptime();
    clock_t delta = uptime - thread->sched.prevBlockCheck;
    if (wasBlocking)
    {
        thread->sched.recentBlockTime = MIN(thread->sched.recentBlockTime + delta, CONFIG_MAX_RECENT_BLOCK_TIME);
    }
    else
    {
        if (delta < thread->sched.recentBlockTime)
        {
            thread->sched.recentBlockTime -= delta;
        }
        else
        {
            thread->sched.recentBlockTime = 0;
        }
    }

    thread->sched.prevBlockCheck = uptime;
}

static void sched_compute_time_slice(thread_t* thread, thread_t* parent)
{
    if (parent != NULL)
    {
        clock_t uptime = timer_uptime();
        clock_t remaining = uptime <= parent->sched.deadline ? parent->sched.deadline - uptime : 0;

        parent->sched.deadline = uptime + remaining / 2;

        thread->sched.timeSlice = remaining / 2;
    }
    else
    {
        priority_t basePriority = atomic_load(&thread->process->priority);
        thread->sched.timeSlice =
            LERP_INT(CONFIG_MIN_TIME_SLICE, CONFIG_MAX_TIME_SLICE, basePriority, PRIORITY_MIN, PRIORITY_MAX);
    }
}

static void sched_compute_actual_priority(thread_t* thread)
{
    priority_t basePriority = atomic_load(&thread->process->priority);

    if (thread->sched.recentBlockTime >= CONFIG_MAX_RECENT_BLOCK_TIME / 2)
    {
        priority_t boost = MIN(CONFIG_MAX_PRIORITY_BOOST, PRIORITY_MAX - 1 - basePriority);
        thread->sched.actualPriority = basePriority +
            LERP_INT(0, boost, thread->sched.recentBlockTime, CONFIG_MAX_RECENT_BLOCK_TIME / 2,
                CONFIG_MAX_RECENT_BLOCK_TIME);
    }
    else
    {
        priority_t penalty = MIN(CONFIG_MAX_PRIORITY_PENALTY, basePriority);
        thread->sched.actualPriority =
            basePriority - LERP_INT(0, penalty, thread->sched.recentBlockTime, 0, CONFIG_MAX_RECENT_BLOCK_TIME / 2);
    }
}

void sched_yield(void)
{
    thread_t* thread = smp_self()->sched.runThread;
    thread->sched.deadline = 0;
    smp_put();
}

SYSCALL_DEFINE(SYS_YIELD, uint64_t)
{
    sched_yield();
    timer_notify_self();
    return 0;
}

static bool sched_should_notify(cpu_t* self, cpu_t* target, priority_t priority)
{
    if (target != self)
    {
        if (target->sched.runThread == target->sched.idleThread)
        {
            return true;
        }
        if (priority > target->sched.runThread->sched.actualPriority)
        {
            return true;
        }
    }

    return false;
}

void sched_push(thread_t* thread, cpu_t* target)
{
    cpu_t* self = smp_self();

    if (target == NULL)
    {
        target = self;
    }

    LOCK_SCOPE(&target->sched.lock);

    thread_state_t state = atomic_exchange(&thread->state, THREAD_READY);
    if (state == THREAD_PARKED)
    {
        sched_queues_push(target->sched.active, thread);
    }
    else if (state == THREAD_UNBLOCKING)
    {
        sched_update_recent_idle_time(thread, true);
        sched_queues_push(target->sched.active, thread);
    }
    else
    {
        panic(NULL, "Invalid thread state for sched_push");
    }

    sched_compute_time_slice(thread, NULL);
    sched_compute_actual_priority(thread);

    if (sched_should_notify(self, target, thread->sched.actualPriority))
    {
        timer_notify(target);
    }

    smp_put();
}

static uint64_t sched_get_load(sched_cpu_ctx_t* ctx)
{
    return ctx->active->length + ctx->expired->length + (ctx->runThread != NULL ? 1 : 0);
}

static cpu_t* sched_find_least_loaded_cpu(cpu_t* exclude)
{
    if (smp_cpu_amount() == 1)
    {
        return smp_cpu(0);
    }

    cpu_t* bestCpu = NULL;
    uint64_t bestLoad = UINT64_MAX;

    // Find the cpu with the best load ;)
    for (uint64_t i = 0; i < smp_cpu_amount(); i++)
    {
        cpu_t* cpu = smp_cpu(i);
        if (cpu == exclude)
        {
            continue;
        }

        uint64_t load = sched_get_load(&cpu->sched);

        if (load < bestLoad)
        {
            bestLoad = load;
            bestCpu = cpu;
        }
    }

    // If given no choice then use the excluded cpu.
    if (bestCpu == NULL)
    {
        bestCpu = exclude;
    }

    return bestCpu;
}

void sched_push_new_thread(thread_t* thread, thread_t* parent)
{
    cpu_t* self = smp_self();

    cpu_t* target = sched_find_least_loaded_cpu(NULL);
    assert(target != NULL);

    LOCK_SCOPE(&target->sched.lock);

    thread_state_t state = atomic_exchange(&thread->state, THREAD_READY);
    if (state == THREAD_PARKED)
    {
        sched_queues_push(target->sched.active, thread);
    }
    else if (state == THREAD_UNBLOCKING)
    {
        sched_update_recent_idle_time(thread, true);
        sched_queues_push(target->sched.active, thread);
    }
    else
    {
        panic(NULL, "Invalid thread state for sched_push");
    }

    sched_compute_time_slice(thread, parent);
    sched_compute_actual_priority(thread);

    if (sched_should_notify(self, target, thread->sched.actualPriority))
    {
        timer_notify(target);
    }

    smp_put();
}

static void sched_load_balance(cpu_t* self, cpu_t* neighbor)
{
    if (smp_cpu_amount() == 1)
    {
        return;
    }

    uint64_t selfLoad = sched_get_load(&self->sched);
    uint64_t neighborLoad = sched_get_load(&neighbor->sched);

    if (selfLoad <= neighborLoad + CONFIG_LOAD_BALANCE_BIAS)
    {
        return;
    }

    bool shouldNotifyNeighbor = false;
    while (selfLoad != neighborLoad)
    {
        thread_t* thread = sched_queues_pop(self->sched.active, PRIORITY_MIN);
        if (thread == NULL)
        {
            break;
        }

        if (sched_should_notify(self, neighbor, thread->sched.actualPriority))
        {
            shouldNotifyNeighbor = true;
        }

        sched_queues_push(neighbor->sched.expired, thread);
        selfLoad--;
        neighborLoad++;
    }

    if (shouldNotifyNeighbor)
    {
        timer_notify(neighbor);
    }
}

static cpu_t* sched_get_neighbor(cpu_t* self)
{
    if (smp_cpu_amount() == 1)
    {
        return NULL;
    }

    // Get the higher neighbor, the last cpu wraps around and gets the first.
    return self->id != smp_cpu_amount() - 1 ? smp_cpu(self->id + 1) : smp_cpu(0);
}

void sched_schedule(interrupt_frame_t* frame, cpu_t* self)
{
    sched_cpu_ctx_t* ctx = &self->sched;

    while (1)
    {
        thread_t* thread = CONTAINER_OF_SAFE(list_pop(&ctx->zombieThreads), thread_t, entry);
        if (thread == NULL)
        {
            break;
        }

        thread_free(thread);
    }

    cpu_t* neighbor = sched_get_neighbor(self);
    // Always use consistent lock ordering to avoid race conditions.
    if (neighbor == NULL)
    {
        lock_acquire(&self->sched.lock);
    }
    else if (neighbor->id > self->id)
    {
        lock_acquire(&self->sched.lock);
        lock_acquire(&neighbor->sched.lock);

        sched_load_balance(self, neighbor);
        lock_release(&neighbor->sched.lock);
    }
    else
    {
        lock_acquire(&neighbor->sched.lock);
        lock_acquire(&self->sched.lock);

        sched_load_balance(self, neighbor);
        lock_release(&neighbor->sched.lock);
    }

    assert(ctx->runThread != NULL);

    sched_update_recent_idle_time(ctx->runThread, false);

    thread_state_t state = atomic_load(&ctx->runThread->state);
    switch (state)
    {
    case THREAD_ZOMBIE:
    {
        assert(ctx->runThread != ctx->idleThread);

        // Push zombie thread to a separate list to deal with later to avoid its stack being freed while its still being
        // used.
        list_push(&ctx->zombieThreads, &ctx->runThread->entry);
        ctx->runThread = NULL; // Force a new thread to be loaded
    }
    break;
    case THREAD_PRE_BLOCK:
    case THREAD_UNBLOCKING:
    {
        assert(ctx->runThread != ctx->idleThread);

        thread_save(ctx->runThread, frame);

        if (wait_block_finalize(frame, self, ctx->runThread)) // Block finalized
        {
            thread_save(ctx->runThread, frame);
            ctx->runThread = NULL; // Force a new thread to be loaded
        }
        else // Early unblock
        {
            atomic_store(&ctx->runThread->state, THREAD_RUNNING);
        }
    }
    break;
    case THREAD_RUNNING:
    {
        // Do nothing
    }
    break;
    default:
    {
        panic(NULL, "Invalid thread state %d (pid=%d tid=%d)", state, ctx->runThread->process->id, ctx->runThread->id);
    }
    }

    clock_t uptime = timer_uptime();

    priority_t minPriority;
    if (ctx->runThread == NULL)
    {
        minPriority = PRIORITY_MIN;
    }
    else if (ctx->runThread == ctx->idleThread)
    {
        minPriority = PRIORITY_MIN;
    }
    else if (ctx->runThread->sched.deadline < uptime)
    {
        minPriority = PRIORITY_MIN;
    }
    else
    {
        minPriority = ctx->runThread->sched.actualPriority;
    }

    if (ctx->active->length == 0)
    {
        sched_queues_t* temp = ctx->active;
        ctx->active = ctx->expired;
        ctx->expired = temp;
    }

    thread_t* next = sched_queues_pop(ctx->active, minPriority);

    if (next == NULL)
    {
        if (ctx->runThread == NULL)
        {
            thread_state_t oldState = atomic_exchange(&ctx->idleThread->state, THREAD_RUNNING);
            assert(oldState == THREAD_READY);
            thread_load(ctx->idleThread, frame);
            ctx->runThread = ctx->idleThread;
        }
    }
    else
    {
        if (ctx->runThread != NULL)
        {
            thread_state_t oldState = atomic_exchange(&ctx->runThread->state, THREAD_READY);
            assert(oldState == THREAD_RUNNING);
            thread_save(ctx->runThread, frame);

            if (ctx->runThread != ctx->idleThread)
            {
                sched_compute_time_slice(ctx->runThread, NULL);
                sched_compute_actual_priority(ctx->runThread);
                sched_queues_push(ctx->expired, ctx->runThread);
            }
        }

        next->sched.deadline = uptime + next->sched.timeSlice;
        thread_state_t oldState = atomic_exchange(&next->state, THREAD_RUNNING);
        assert(oldState == THREAD_READY);
        thread_load(next, frame);
        ctx->runThread = next;
    }

    if (ctx->runThread != ctx->idleThread && ctx->runThread->sched.deadline > uptime)
    {
        timer_one_shot(self, uptime, ctx->runThread->sched.deadline - uptime);
    }

    lock_release(&self->sched.lock);
}
