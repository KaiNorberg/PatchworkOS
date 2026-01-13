#include <errno.h>
#include <kernel/cpu/cpu.h>

#include <kernel/cpu/gdt.h>
#include <kernel/cpu/idt.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/cpu/ipi.h>
#include <kernel/cpu/simd.h>
#include <kernel/cpu/syscall.h>
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

void cpu_init_early(cpu_t* cpu)
{
    cpu_id_t id = _cpuAmount++;
    _cpus[id] = cpu;

    cpu->self = cpu;
    cpu->id = id;
    cpu->syscallRsp = 0;
    cpu->userRsp = 0;
    cpu->inInterrupt = false;
    gdt_cpu_load();
    idt_cpu_load();
    tss_init(&cpu->tss);

    gdt_cpu_tss_load(&cpu->tss);
    msr_write(MSR_GS_BASE, (uintptr_t)cpu);
    msr_write(MSR_KERNEL_GS_BASE, (uintptr_t)cpu);

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

    if (stack_pointer_init_buffer(&cpu->nmiStack, cpu->nmiStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) == ERR)
    {
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        stack_pointer_deinit_buffer(&cpu->doubleFaultStack);
        panic(NULL, "Failed to init NMI stack for cpu %u\n", cpu->id);
    }
    *(uint64_t*)cpu->nmiStack.bottom = CPU_STACK_CANARY;
    tss_ist_load(&cpu->tss, TSS_IST_NMI, &cpu->nmiStack);

    if (stack_pointer_init_buffer(&cpu->interruptStack, cpu->interruptStackBuffer, CONFIG_INTERRUPT_STACK_PAGES) == ERR)
    {
        stack_pointer_deinit_buffer(&cpu->exceptionStack);
        stack_pointer_deinit_buffer(&cpu->doubleFaultStack);
        panic(NULL, "Failed to init interrupt stack for cpu %u\n", cpu->id);
    }
    *(uint64_t*)cpu->interruptStack.bottom = CPU_STACK_CANARY;
    tss_ist_load(&cpu->tss, TSS_IST_INTERRUPT, &cpu->interruptStack);

    memset(cpu->percpu, 0, CONFIG_PERCPU_SIZE);
}

void cpu_init(cpu_t* cpu)
{
    ipi_cpu_init(&cpu->ipi);

    percpu_update();
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
    if (*(uint64_t*)cpu->nmiStack.bottom != CPU_STACK_CANARY)
    {
        panic(NULL, "CPU%u NMI stack overflow detected", cpu->id);
    }
    if (*(uint64_t*)cpu->interruptStack.bottom != CPU_STACK_CANARY)
    {
        panic(NULL, "CPU%u interrupt stack overflow detected", cpu->id);
    }
}

static void cpu_halt_ipi_handler(ipi_func_data_t* data)
{
    UNUSED(data);

    while (true)
    {
        asm volatile("cli; hlt");
    }

    __builtin_unreachable();
}

uint64_t cpu_halt_others(void)
{
    if (ipi_send(cpu_get(), IPI_OTHERS, cpu_halt_ipi_handler, NULL) == ERR)
    {
        return ERR;
    }
    return 0;
}

uintptr_t cpu_interrupt_stack_top(cpu_t* cpu)
{
    return cpu->interruptStack.top;
}