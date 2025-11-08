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
#include <stdint.h>
#include <sys/list.h>

cpu_t* _cpus[CPU_MAX] = {0};
uint16_t _cpuAmount = 0;

void cpu_identify(cpu_t* cpu)
{
    gdt_cpu_load();
    idt_cpu_load();

    cpuid_t id = _cpuAmount++;
    msr_write(MSR_CPU_ID, (uint64_t)id);

    cpu->id = id;
    _cpus[id] = cpu;
}

uint64_t cpu_init(cpu_t* cpu)
{    
    simd_cpu_init();
    syscalls_cpu_init();

    tss_init(&cpu->tss);
    vmm_cpu_ctx_init(&cpu->vmm);
    gdt_cpu_tss_load(&cpu->tss);
    interrupt_ctx_init(&cpu->interrupt);
    perf_cpu_ctx_init(&cpu->perf);
    timer_cpu_ctx_init(&cpu->timer);
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

_NORETURN void cpu_halt(void)
{
    asm volatile("cli; hlt");

    __builtin_unreachable();
}

void cpu_halt_others(void)
{
    cpu_t* self = cpu_get_unsafe();

    cpu_t* cpu;
    CPU_FOR_EACH(cpu)
    {
        if (cpu == self)
        {
            continue;
        }

        lapic_send_ipi(cpu->lapicId, INTERRUPT_HALT);
    }
}