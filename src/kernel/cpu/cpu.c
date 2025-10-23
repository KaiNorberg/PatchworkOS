#include "cpu.h"

#include "drivers/apic.h"
#include "drivers/statistics.h"
#include "gdt.h"
#include "idt.h"
#include "interrupt.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "sched/sched.h"
#include "simd.h"
#include "syscalls.h"
#include "tss.h"

uint64_t cpu_init(cpu_t* cpu, cpuid_t id)
{
    gdt_cpu_load();
    idt_cpu_load();

    msr_write(MSR_CPU_ID, id);

    cpu->id = id;
    cpu->lapicId = lapic_self_id();
    tss_init(&cpu->tss);
    vmm_cpu_ctx_init(&cpu->vmm);
    gdt_cpu_tss_load(&cpu->tss);
    interrupt_ctx_init(&cpu->interrupt);
    statistics_cpu_ctx_init(&cpu->stat);
    timer_ctx_init(&cpu->timer);
    wait_cpu_ctx_init(&cpu->wait, cpu);
    sched_cpu_ctx_init(&cpu->sched, cpu);

    if (stack_pointer_init_buffer(&cpu->exceptionStack, cpu->exceptionStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) == ERR)
    {
        LOG_ERR("failed to init exception stack for cpu %u\n", cpu->id);
        return ERR;
    }
    tss_ist_load(&cpu->tss, TSS_IST_EXCEPTION, &cpu->exceptionStack);
    *(uint64_t*)cpu->exceptionStack.bottom = CPU_STACK_CANARY;

    if (stack_pointer_init_buffer(&cpu->doubleFaultStack, cpu->doubleFaultStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) ==
        ERR)
    {
        LOG_ERR("failed to init double fault stack for cpu %u\n", cpu->id);
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        return ERR;
    }
    tss_ist_load(&cpu->tss, TSS_IST_DOUBLE_FAULT, &cpu->doubleFaultStack);
    *(uint64_t*)cpu->doubleFaultStack.bottom = CPU_STACK_CANARY;

    if (stack_pointer_init_buffer(&cpu->interruptStack, cpu->interruptStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) == ERR)
    {
        LOG_ERR("failed to init interrupt stack for cpu %u\n", cpu->id);
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        stack_pointer_deinit_buffer(&cpu->doubleFaultStack);
        return ERR;
    }
    tss_ist_load(&cpu->tss, TSS_IST_INTERRUPT, &cpu->interruptStack);
    *(uint64_t*)cpu->interruptStack.bottom = CPU_STACK_CANARY;

    lapic_cpu_init();
    simd_cpu_init();
    syscalls_cpu_init();

    return 0;
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
