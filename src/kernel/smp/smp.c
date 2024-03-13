#include "smp.h"

#include <libc/string.h>

#include "tty/tty.h"
#include "registers/registers.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "utils/utils.h"
#include "smp/trampoline/trampoline.h"
#include "smp/startup/startup.h"
#include "interrupts/interrupts.h"

static Cpu cpus[MAX_CPU_AMOUNT];
static uint8_t cpuAmount;

void smp_init(void)
{
    tty_start_message("SMP initializing");

    memset(cpus, 0, sizeof(Cpu) * MAX_CPU_AMOUNT);
    cpuAmount = 0;

    smp_trampoline_setup();
    smp_startup(cpus, &cpuAmount);
    smp_trampoline_cleanup();

    tty_end_message(TTY_MESSAGE_OK);
}

void smp_send_ipi(Cpu const* cpu, uint8_t ipi)
{
    local_apic_send_ipi(cpu->localApicId, IPI_BASE + ipi);
}

void smp_send_ipi_to_others(uint8_t ipi)
{
    Cpu const* self = smp_self_unsafe();
    for (uint8_t id = 0; id < cpuAmount; id++)
    {
        if (self->id != id)
        {
            smp_send_ipi(&cpus[id], ipi);
        }
    }
}

uint8_t smp_cpu_amount(void)
{
    return cpuAmount;
}

Cpu const* smp_cpu(uint8_t id)
{
    return &cpus[id];
}

Cpu* smp_self(void)
{
    interrupts_disable();

    return &cpus[msr_read(MSR_CPU_ID)];
}

Cpu* smp_self_unsafe(void)
{
    if (rflags_read() & RFLAGS_INTERRUPT_ENABLE)
    {
        debug_panic("smp_self_unsafe called with interrupts enabled");
    }

    return &cpus[msr_read(MSR_CPU_ID)];
}

Cpu* smp_self_brute(void)
{
    if (rflags_read() & RFLAGS_INTERRUPT_ENABLE)
    {
        debug_panic("smp_self_brute called with interrupts enabled");
    }

    uint8_t localApicId = local_apic_id();
    for (uint16_t id = 0; id < cpuAmount; id++)
    {
        Cpu* cpu = &cpus[id];

        if (cpu->present && cpu->localApicId == localApicId)
        {
            return cpu;
        }
    }    

    debug_panic("Unable to find cpu");
    return 0;
}

void smp_put(void)
{
    interrupts_enable();
}