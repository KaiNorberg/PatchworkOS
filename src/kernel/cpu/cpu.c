#include "cpu.h"

#include "drivers/apic.h"
#include "drivers/hpet.h"
#include "init/init.h"
#include "log/log.h"
#include "sched/sched.h"
#include "sched/thread.h"
#include "trampoline.h"
#include "trap.h"
#include "tss.h"
#include "utils/statistics.h"

uint64_t cpu_init(cpu_t* cpu, cpuid_t id, uint8_t lapicId)
{
    cpu->id = id;
    cpu->lapicId = lapicId;
    cpu->trapDepth = 0;
    tss_init(&cpu->tss);
    cli_ctx_init(&cpu->cli);
    statistics_cpu_ctx_init(&cpu->stat);
    tss_ist_load(&cpu->tss, TSS_IST_EXCEPTION, &cpu->exceptionStack);
    tss_ist_load(&cpu->tss, TSS_IST_DOUBLE_FAULT, &cpu->doubleFaultStack);
    timer_ctx_init(&cpu->timer);
    wait_cpu_ctx_init(&cpu->wait);
    sched_cpu_ctx_init(&cpu->sched, cpu);

    if (stack_pointer_init_buffer(&cpu->exceptionStack, cpu->exceptionStackBuffer, CONFIG_EXCEPTION_STACK_PAGES) == ERR)
    {
        return ERR;
    }
    tss_ist_load(&cpu->tss, TSS_IST_EXCEPTION, &cpu->exceptionStack);

    if (stack_pointer_init_buffer(&cpu->doubleFaultStack, cpu->doubleFaultStackBuffer, CONFIG_EXCEPTION_STACK_PAGES) ==
        ERR)
    {
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        return ERR;
    }
    tss_ist_load(&cpu->tss, TSS_IST_DOUBLE_FAULT, &cpu->doubleFaultStack);

    return 0;
}

void cpu_entry(cpuid_t id)
{
    msr_write(MSR_CPU_ID, id);

    init_other_cpu();

    trampoline_signal_ready();

    sched_idle_loop();
}

uint64_t cpu_start(cpu_t* cpu)
{
    assert(cpu->sched.idleThread != NULL);

    if (trampoline_cpu_setup(cpu->id, cpu->sched.idleThread->kernelStack.top, cpu_entry) != 0)
    {
        LOG_ERR("failed to setup trampoline for cpu %u\n", cpu->id);
        return ERR;
    }

    lapic_send_init(cpu->lapicId);
    hpet_wait(CLOCKS_PER_SEC / 100);
    lapic_send_sipi(cpu->lapicId, (void*)TRAMPOLINE_BASE_ADDR);

    if (trampoline_wait_ready(cpu->id, CLOCKS_PER_SEC) != 0)
    {
        LOG_ERR("cpu %d timed out\n", cpu->id);
        return ERR;
    }

    return 0;
}
