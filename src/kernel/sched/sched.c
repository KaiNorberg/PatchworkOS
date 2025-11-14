#include <kernel/cpu/irq.h>
#include <kernel/sched/sched.h>

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/gdt.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/syscalls.h>
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

static wait_queue_t sleepQueue = WAIT_QUEUE_CREATE(sleepQueue);

static irq_handler_t* dieHandler = NULL;
static irq_handler_t* scheduleHandler = NULL;

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
    list_push_back(&queues->lists[thread->sched.actualPriority], &thread->entry);
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
    thread_t* thread = CONTAINER_OF(list_pop_first(&queues->lists[highestPriority]), thread_t, entry);

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

static void sched_timer_handler(interrupt_frame_t* frame, cpu_t* self)
{
    sched_invoke(frame, self, SCHED_NORMAL);
}

void sched_cpu_ctx_init(sched_cpu_ctx_t* ctx, cpu_t* self)
{
    sched_queues_init(&ctx->queues[0]);
    sched_queues_init(&ctx->queues[1]);
    ctx->active = &ctx->queues[0];
    ctx->expired = &ctx->queues[1];

    // Bootstrap cpu is initalized early so we cant yet create the idle thread, the boot thread on the bootstrap cpu
    // will become the idle thread.
    if (self->id == CPU_ID_BOOTSTRAP)
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
    ctx->owner = self;
}

static void sched_die_irq_handler(irq_func_data_t* data)
{
    sched_invoke(data->frame, data->self, SCHED_DIE);
}

static void sched_schedule_irq_handler(irq_func_data_t* data)
{
    sched_invoke(data->frame, data->self, SCHED_NORMAL);
}

void sched_init(void)
{
    dieHandler = irq_handler_register(IRQ_VIRT_DIE, sched_die_irq_handler, NULL);
    if (dieHandler == NULL)
    {
        panic(NULL, "failed to register die IRQ handler");
    }

    scheduleHandler = irq_handler_register(IRQ_VIRT_SCHEDULE, sched_schedule_irq_handler, NULL);
    if (scheduleHandler == NULL)
    {
        panic(NULL, "failed to register sched IRQ handler");
    }
}

void sched_done_with_boot_thread(void)
{
    cpu_t* self = cpu_get_unsafe();
    sched_cpu_ctx_t* ctx = &self->sched;

    assert(self->id == CPU_ID_BOOTSTRAP && ctx->runThread->process == process_get_kernel() && ctx->runThread->id == 0);

    // The boot thread becomes the bootstrap cpus idle thread.
    ctx->runThread->sched.deadline = 0;
    ctx->idleThread = ctx->runThread;

    if (timer_source_amount() == 0)
    {
        panic(NULL, "No timer source registered, cannot continue");
    }

    timer_set(self, sys_time_uptime(), 0); // Set timer to trigger in the idle loop.
    asm volatile("sti");
    // When we return here the boot thread will be an idle thread so we just enter the idle loop.
    sched_idle_loop();
}

uint64_t sched_nanosleep(clock_t timeout)
{
    return WAIT_BLOCK_TIMEOUT(&sleepQueue, false, timeout);
}

SYSCALL_DEFINE(SYS_NANOSLEEP, uint64_t, clock_t nanoseconds)
{
    return sched_nanosleep(nanoseconds);
}

bool sched_is_idle(cpu_t* cpu)
{
    sched_cpu_ctx_t* ctx = &cpu->sched;
    LOCK_SCOPE(&ctx->lock);
    bool isIdle = ctx->runThread == ctx->idleThread;
    return isIdle;
}

thread_t* sched_thread(void)
{
    thread_t* thread = cpu_get()->sched.runThread;
    cpu_put();
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

thread_t* sched_thread_unsafe(void)
{
    return cpu_get_unsafe()->sched.runThread;
}

process_t* sched_process_unsafe(void)
{
    thread_t* thread = sched_thread_unsafe();
    if (thread == NULL)
    {
        return NULL;
    }

    return thread->process;
}

void sched_process_exit(uint64_t status)
{
    thread_t* thread = sched_thread();
    process_kill(thread->process, status);
    IRQ_INVOKE(IRQ_VIRT_DIE);
    panic(NULL, "Return to sched_process_exit");
}

SYSCALL_DEFINE(SYS_PROCESS_EXIT, void, uint64_t status)
{
    sched_process_exit(status);
    panic(NULL, "Return to syscall_process_exit");
}

void sched_thread_exit(void)
{
    IRQ_INVOKE(IRQ_VIRT_DIE);
    panic(NULL, "Return to sched_thread_exit");
}

SYSCALL_DEFINE(SYS_THREAD_EXIT, void)
{
    sched_thread_exit();
    panic(NULL, "Return to syscall_thread_exit");
}

static void sched_update_recent_idle_time(thread_t* thread, bool wasBlocking, clock_t uptime)
{
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
        clock_t uptime = sys_time_uptime();
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
    thread_t* thread = cpu_get()->sched.runThread;
    thread->sched.deadline = 0;
    cpu_put();
    IRQ_INVOKE(IRQ_VIRT_SCHEDULE);
}

SYSCALL_DEFINE(SYS_YIELD, uint64_t)
{
    sched_yield();
    return 0;
}

static bool sched_should_notify(cpu_t* target, priority_t priority)
{
    if (target->sched.runThread == target->sched.idleThread)
    {
        return true;
    }
    if (priority > target->sched.runThread->sched.actualPriority)
    {
        return true;
    }

    return false;
}

void sched_push(thread_t* thread, cpu_t* target)
{
    if (target == NULL)
    {
        target = cpu_get();
        lock_acquire(&target->sched.lock);
        cpu_put();
    }
    else
    {
        lock_acquire(&target->sched.lock);
    }

    thread_state_t state = atomic_exchange(&thread->state, THREAD_READY);
    if (state == THREAD_PARKED)
    {
        sched_queues_push(target->sched.active, thread);
    }
    else if (state == THREAD_UNBLOCKING)
    {
        sched_update_recent_idle_time(thread, true, sys_time_uptime());
        sched_queues_push(target->sched.active, thread);
    }
    else
    {
        lock_release(&target->sched.lock);
        panic(NULL, "Invalid thread state for sched_push");
    }

    sched_compute_time_slice(thread, NULL);
    sched_compute_actual_priority(thread);

    bool shouldNotify = sched_should_notify(target, thread->sched.actualPriority);

    lock_release(&target->sched.lock);

    if (shouldNotify)
    {
        ipi_invoke(target, IRQ_VIRT_SCHEDULE);
    }
}

static uint64_t sched_get_load(sched_cpu_ctx_t* ctx)
{
    LOCK_SCOPE(&ctx->lock);
    return ctx->active->length + ctx->expired->length + (ctx->runThread != ctx->idleThread ? 1 : 0);
}

static cpu_t* sched_find_least_loaded_cpu(cpu_t* exclude)
{
    if (cpu_amount() == 1)
    {
        return cpu_get_unsafe();
    }

    cpu_t* bestCpu = NULL;
    uint64_t bestLoad = UINT64_MAX;

    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
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
    cpu_get();

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
        sched_update_recent_idle_time(thread, true, sys_time_uptime());
        sched_queues_push(target->sched.active, thread);
    }
    else
    {
        panic(NULL, "Invalid thread state for sched_push");
    }

    sched_compute_time_slice(thread, parent);
    sched_compute_actual_priority(thread);

    if (sched_should_notify(target, thread->sched.actualPriority))
    {
        ipi_invoke(target, IRQ_VIRT_SCHEDULE);
    }

    cpu_put();
}

static cpu_t* sched_get_neighbor(cpu_t* self)
{
    cpu_t* next = cpu_get_next(self);
    return next != self ? next : NULL;
}

static void sched_load_balance(cpu_t* self)
{
    // Technically there are race conditions here, but the worst case scenario is imperfect load balancing
    // and we need to avoid holding the locks of two sched_cpu_ctx_t at the same time to prevent deadlocks.

    if (cpu_amount() == 1)
    {
        return;
    }

    cpu_t* neighbor = sched_get_neighbor(self);

    uint64_t selfLoad = sched_get_load(&self->sched);
    uint64_t neighborLoad = sched_get_load(&neighbor->sched);

    if (selfLoad <= neighborLoad + CONFIG_LOAD_BALANCE_BIAS)
    {
        return;
    }

    bool shouldNotifyNeighbor = false;
    while (selfLoad != neighborLoad)
    {
        lock_acquire(&self->sched.lock);
        thread_t* thread = sched_queues_pop(self->sched.active, PRIORITY_MIN);
        lock_release(&self->sched.lock);
        if (thread == NULL)
        {
            break;
        }

        if (sched_should_notify(neighbor, thread->sched.actualPriority))
        {
            shouldNotifyNeighbor = true;
        }

        lock_acquire(&neighbor->sched.lock);
        sched_queues_push(neighbor->sched.expired, thread);
        lock_release(&neighbor->sched.lock);
        selfLoad--;
        neighborLoad++;
    }

    if (shouldNotifyNeighbor)
    {
        ipi_invoke(neighbor, IRQ_VIRT_SCHEDULE);
    }
}

void sched_invoke(interrupt_frame_t* frame, cpu_t* self, schedule_flags_t flags)
{
    sched_cpu_ctx_t* ctx = &self->sched;
    sched_load_balance(self);

    lock_acquire(&ctx->lock);

    thread_t* volatile runThread = ctx->runThread; // Prevent the compiler from being annoying.
    if (runThread == NULL)
    {
        lock_release(&ctx->lock);
        panic(NULL, "runThread is NULL");
    }

    clock_t uptime = sys_time_uptime();
    sched_update_recent_idle_time(runThread, false, uptime);

    thread_t* volatile threadToFree = NULL;
    if (flags & SCHED_DIE)
    {
        assert(atomic_load(&runThread->state) == THREAD_RUNNING);

        threadToFree = runThread;
        runThread = NULL;
        LOG_DEBUG("dying tid=%d pid=%d\n", threadToFree->id, threadToFree->process->id);
    }
    else
    {
        thread_state_t state = atomic_load(&runThread->state);
        switch (state)
        {
        case THREAD_PRE_BLOCK:
        case THREAD_UNBLOCKING:
        {
            assert(runThread != ctx->idleThread);

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
        default:
        {
            panic(NULL, "Invalid thread state %d (pid=%d tid=%d)", state, runThread->process->id, runThread->id);
        }
        }
    }

    priority_t minPriority;
    if (runThread == NULL)
    {
        minPriority = PRIORITY_MIN;
    }
    else if (runThread == ctx->idleThread)
    {
        minPriority = PRIORITY_MIN;
    }
    else if (runThread->sched.deadline < uptime)
    {
        minPriority = PRIORITY_MIN;
    }
    else
    {
        minPriority = runThread->sched.actualPriority;
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
        if (runThread == NULL)
        {
            thread_state_t oldState = atomic_exchange(&ctx->idleThread->state, THREAD_RUNNING);
            assert(oldState == THREAD_READY);
            thread_load(ctx->idleThread, frame);
            runThread = ctx->idleThread;
        }
    }
    else
    {
        if (runThread != NULL)
        {
            thread_state_t oldState = atomic_exchange(&runThread->state, THREAD_READY);
            assert(oldState == THREAD_RUNNING);
            thread_save(runThread, frame);

            if (runThread != ctx->idleThread)
            {
                sched_compute_time_slice(runThread, NULL);
                sched_compute_actual_priority(runThread);
                sched_queues_push(ctx->expired, runThread);
            }
        }

        next->sched.deadline = uptime + next->sched.timeSlice;
        thread_state_t oldState = atomic_exchange(&next->state, THREAD_RUNNING);
        assert(oldState == THREAD_READY);
        thread_load(next, frame);
        runThread = next;
    }

    if (runThread != ctx->idleThread && runThread->sched.deadline > uptime)
    {
        timer_set(self, uptime, runThread->sched.deadline - uptime);
    }

    if (threadToFree != NULL)
    {
        thread_free(threadToFree);
    }

    ctx->runThread = runThread;
    lock_release(&ctx->lock);
}
