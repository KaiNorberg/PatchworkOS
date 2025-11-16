#include <errno.h>
#include <kernel/cpu/cpu.h>

#include <kernel/cpu/gdt.h>
#include <kernel/cpu/idt.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/simd.h>
#include <kernel/cpu/syscalls.h>
#include <kernel/cpu/tss.h>
#include <kernel/drivers/perf.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sched/sched.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rwlock.h>
#include <kernel/utils/map.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/list.h>

cpu_t* _cpus[CPU_MAX] = {0};
uint16_t _cpuAmount = 0;

static cpu_handler_t eventHandlers[CPU_MAX_EVENT_HANDLERS] = {0};
static uint64_t eventHandlerCount = 0;
static lock_t eventHandlerLock = LOCK_CREATE;

void cpu_init_early(cpu_t* cpu)
{
    cpuid_t id = _cpuAmount++;
    msr_write(MSR_CPU_ID, (uint64_t)id);

    cpu->id = id;
    _cpus[id] = cpu;

    gdt_cpu_load();
    idt_cpu_load();
    tss_init(&cpu->tss);
    gdt_cpu_tss_load(&cpu->tss);

    if (stack_pointer_init_buffer(&cpu->exceptionStack, cpu->exceptionStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) == ERR)
    {
        panic(NULL, "Failed to init exception stack for cpu %u\n", cpu->id);
    }
    *(uint64_t*)cpu->exceptionStack.bottom = CPU_STACK_CANARY;
    tss_ist_load(&cpu->tss, TSS_IST_EXCEPTION, &cpu->exceptionStack);

    if (stack_pointer_init_buffer(&cpu->doubleFaultStack, cpu->doubleFaultStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) ==
        ERR)
    {
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        panic(NULL, "Failed to init double fault stack for cpu %u\n", cpu->id);
    }
    *(uint64_t*)cpu->doubleFaultStack.bottom = CPU_STACK_CANARY;
    tss_ist_load(&cpu->tss, TSS_IST_DOUBLE_FAULT, &cpu->doubleFaultStack);

    if (stack_pointer_init_buffer(&cpu->interruptStack, cpu->interruptStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) == ERR)
    {
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        stack_pointer_deinit_buffer(&cpu->doubleFaultStack);
        panic(NULL, "Failed to init interrupt stack for cpu %u\n", cpu->id);
    }
    *(uint64_t*)cpu->interruptStack.bottom = CPU_STACK_CANARY;
    tss_ist_load(&cpu->tss, TSS_IST_INTERRUPT, &cpu->interruptStack);   
}

void cpu_init(cpu_t* cpu)
{
    simd_cpu_init();
    syscalls_cpu_init();

    vmm_cpu_ctx_init(&cpu->vmm);
    interrupt_ctx_init(&cpu->interrupt);
    perf_cpu_ctx_init(&cpu->perf);
    timer_cpu_ctx_init(&cpu->timer);
    wait_cpu_ctx_init(&cpu->wait, cpu);
    sched_cpu_ctx_init(&cpu->sched, cpu);
    ipi_cpu_ctx_init(&cpu->ipi);

    LOCK_SCOPE(&eventHandlerLock);
    for (uint64_t i = 0; i < eventHandlerCount; i++)
    {
        cpu_event_t event = { .type = CPU_ONLINE };
        eventHandlers[i].func(cpu, &event);
        bitmap_set(&eventHandlers[i].initializedCpus, cpu->id);
    }
}

uint64_t cpu_handler_register(cpu_func_t func)
{
    if (func == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&eventHandlerLock);
    if (eventHandlerCount >= CPU_MAX_EVENT_HANDLERS)
    {
        errno = EBUSY;
        return ERR;
    }

    for (uint64_t i = 0; i < eventHandlerCount; i++)
    {
        if (eventHandlers[i].func == func)
        {
            errno = EEXIST;
            return ERR;
        }
    }

    cpu_handler_t* eventHandler = &eventHandlers[eventHandlerCount++];
    eventHandler->func = func;
    BITMAP_DEFINE_INIT(eventHandler->initializedCpus, CPU_MAX);
    bitmap_clear_range(&eventHandler->initializedCpus, 0, CPU_MAX);

    cpu_t* self = cpu_get_unsafe();
    if (self != NULL)
    {
        cpu_event_t event = { .type = CPU_ONLINE };
        eventHandler->func(self, &event);
        bitmap_set(&eventHandler->initializedCpus, self->id);
    }

    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
        if (cpu == self)
        {
            continue;
        }
        atomic_store(&cpu->needHandlersCheck, true);
    }

    return 0;
}

void cpu_handler_unregister(cpu_func_t func)
{
    if (func == NULL)
    {
        return;
    }

    LOCK_SCOPE(&eventHandlerLock);

    for (uint64_t i = 0; i < eventHandlerCount; i++)
    {
        cpu_handler_t* eventHandler = &eventHandlers[i];
        if (eventHandler->func == func)
        {
            eventHandler->func = NULL;
            eventHandlerCount--;
            memmove(&eventHandlers[i], &eventHandlers[i + 1],
                (CPU_MAX_EVENT_HANDLERS - i - 1) * sizeof(cpu_func_t));
            break;
        }
    }
}

void cpu_handlers_check(cpu_t* cpu)
{
    bool expected = true;
    if (!atomic_compare_exchange_strong(&cpu->needHandlersCheck, &expected, false))
    {
        return;
    }

    LOCK_SCOPE(&eventHandlerLock);
    for (uint64_t i = 0; i < eventHandlerCount; i++)
    {
        cpu_handler_t* eventHandler = &eventHandlers[i];
        if (!bitmap_is_set(&eventHandler->initializedCpus, cpu->id))
        {
            cpu_event_t event = { .type = CPU_ONLINE };
            eventHandler->func(cpu, &event);
            bitmap_set(&eventHandler->initializedCpus, cpu->id);
        }
    }
} 

void cpu_stacks_overflow_check(cpu_t* cpu)
{
    if (*(uint64_t*)cpu->exceptionStack.bottom != CPU_STACK_CANARY)
    {
        panic(NULL, "CPU%u exception stack overflow detected", cpu->id);
    }
    if (*(uint64_t*)cpu->doubleFaultStack.bottom != CPU_STACK_CANARY)
    {
        panic(NULL, "CPU%u double fault stack overflow detected", cpu->id);
    }
    if (*(uint64_t*)cpu->interruptStack.bottom != CPU_STACK_CANARY)
    {
        panic(NULL, "CPU%u interrupt stack overflow detected", cpu->id);
    }
}

static void cpu_halt_ipi_handler(irq_func_data_t* data)
{
    (void)data;

    while (true)
    {
        asm volatile("cli; hlt");
    }

    __builtin_unreachable();
}

uint64_t cpu_halt_others(void)
{
    if (ipi_send(cpu_get_unsafe(), IPI_OTHERS, cpu_halt_ipi_handler, NULL) == ERR)
    {
        return ERR;
    }
    return 0;
}