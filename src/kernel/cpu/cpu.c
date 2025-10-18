#include "cpu.h"

#include "drivers/apic.h"
#include "gdt.h"
#include "idt.h"
#include "interrupt.h"
#include "log/log.h"
#include "mem/vmm.h"
#include "sched/sched.h"
#include "simd.h"
#include "syscalls.h"
#include "tss.h"
#include "utils/statistics.h"

uint64_t cpu_init(cpu_t* cpu, cpuid_t id)
{
    gdt_cpu_load();
    idt_cpu_load();

    msr_write(MSR_CPU_ID, id);
    cpu->id = id;
    cpu->lapicId = lapic_self_id();

    tss_init(&cpu->tss);
    gdt_cpu_tss_load(&cpu->tss);
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

    lapic_cpu_init();
    simd_cpu_init();
    vmm_cpu_init();
    syscalls_cpu_init();

    return 0;
}
