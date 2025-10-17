#include "cpu.h"

#include "drivers/apic.h"
#include "init/init.h"
#include "interrupt.h"
#include "log/log.h"
#include "sched/sched.h"
#include "trampoline.h"
#include "tss.h"
#include "utils/statistics.h"

uint64_t cpu_init(cpu_t* cpu, cpuid_t id, uint8_t lapicId)
{
    cpu->id = id;
    cpu->lapicId = lapicId;
    tss_init(&cpu->tss);
    interrupt_ctx_init(&cpu->interrupt);
    statistics_cpu_ctx_init(&cpu->stat);
    timer_ctx_init(&cpu->timer);
    wait_cpu_ctx_init(&cpu->wait);
    sched_cpu_ctx_init(&cpu->sched, cpu);

    if (stack_pointer_init_buffer(&cpu->exceptionStack, cpu->exceptionStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) == ERR)
    {
        LOG_ERR("failed to init exception stack for cpu %u\n", cpu->id);
        return ERR;
    }
    tss_ist_load(&cpu->tss, TSS_IST_EXCEPTION, &cpu->exceptionStack);
    memset(cpu->exceptionStackBuffer, 0, sizeof(cpu->exceptionStackBuffer));

    if (stack_pointer_init_buffer(&cpu->doubleFaultStack, cpu->doubleFaultStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) ==
        ERR)
    {
        LOG_ERR("failed to init double fault stack for cpu %u\n", cpu->id);
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        return ERR;
    }
    tss_ist_load(&cpu->tss, TSS_IST_DOUBLE_FAULT, &cpu->doubleFaultStack);
    memset(cpu->doubleFaultStackBuffer, 0, sizeof(cpu->doubleFaultStackBuffer));

    if (stack_pointer_init_buffer(&cpu->interruptStack, cpu->interruptStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) == ERR)
    {
        LOG_ERR("failed to init interrupt stack for cpu %u\n", cpu->id);
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        stack_pointer_deinit_buffer(&cpu->doubleFaultStack);
        return ERR;
    }
    tss_ist_load(&cpu->tss, TSS_IST_INTERRUPT, &cpu->interruptStack);
    memset(cpu->interruptStackBuffer, 0, sizeof(cpu->interruptStackBuffer));

    return 0;
}

uint64_t cpu_start(cpu_t* cpu)
{
    assert(cpu->sched.idleThread != NULL);
    trampoline_send_startup_ipi(cpu);

    if (trampoline_wait_ready(cpu->id, CLOCKS_PER_SEC) == ERR)
    {
        LOG_ERR("cpu %d timed out\n", cpu->id);
        return ERR;
    }

    return 0;
}
