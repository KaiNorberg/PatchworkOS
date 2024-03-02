#include "smp.h"

#include <libc/string.h>

#include "tty/tty.h"
#include "apic/apic.h"
#include "debug/debug.h"
#include "utils/utils.h"
#include "smp/trampoline/trampoline.h"
#include "smp/startup/startup.h"

static Cpu cpus[MAX_CPU_AMOUNT];
static uint8_t cpuAmount;

void smp_init()
{
    tty_start_message("SMP initializing");

    memset(cpus, 0, sizeof(Cpu) * MAX_CPU_AMOUNT);
    cpuAmount = 0;

    smp_trampoline_setup();
    smp_startup(cpus, &cpuAmount);
    smp_trampoline_cleanup();

    tty_end_message(TTY_MESSAGE_OK);
}

void smp_begin_interrupt(InterruptFrame* interruptFrame)
{
    Cpu* self = smp_self();

    self->interruptFrame = interruptFrame;
}

void smp_end_interrupt()
{
    Cpu* self = smp_self();

    self->interruptFrame = 0;
}

void smp_send_ipi(Cpu* cpu, Ipi ipi)
{
    cpu->ipi = ipi;
    local_apic_send_ipi(cpu->localApicId, IPI_VECTOR);
}

void smp_send_ipi_to_others(Ipi ipi)
{
    for (uint8_t id = 0; id < cpuAmount; id++)
    {
        Cpu* cpu = smp_cpu(id);

        if (cpu->localApicId != local_apic_id())
        {
            smp_send_ipi(cpu, ipi);
        }
    }
}

void smp_send_ipi_to_all(Ipi ipi)
{
    smp_send_ipi_to_others(ipi);
    smp_send_ipi(smp_self(), ipi);
}

Ipi smp_receive_ipi()
{
    Cpu* self = smp_self();

    Ipi temp = self->ipi;
    self->ipi = (Ipi){.type = IPI_TYPE_NONE};

    return temp;
}

uint8_t smp_cpu_amount()
{
    return cpuAmount;
}

Cpu* smp_cpu(uint8_t id)
{
    return &cpus[id];
}

Cpu* smp_self()
{
    uint64_t id = read_msr(MSR_CPU_ID);
    return &cpus[id];
}

Cpu* smp_self_brute()
{
    uint8_t localApicId = local_apic_id();
    for (uint16_t i = 0; i < cpuAmount; i++)
    {
        Cpu* cpu = smp_cpu((uint8_t)i);

        if (cpu->present && cpu->localApicId == localApicId)
        {
            return cpu;
        }
    }    

    debug_panic("Unable to find worker");
    return 0;
}