#include "smp.h"

#include <string.h>

#include "tty/tty.h"
#include "regs/regs.h"
#include "heap/heap.h"
#include "apic/apic.h"
#include "madt/madt.h"
#include "debug/debug.h"
#include "utils/utils.h"
#include "smp/trampoline/trampoline.h"
#include "smp/startup/startup.h"

static Cpu* cpus;
static uint8_t cpuAmount = 0;

static bool initialized = false;

static void smp_detect_cpus()
{
    LocalApicRecord* record = madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
    while (record != 0)
    {
        if (LOCAL_APIC_RECORD_GET_FLAG(record, LOCAL_APIC_RECORD_FLAG_ENABLEABLE))
        {
            cpuAmount++;
        }

        record = madt_next_record(record, MADT_RECORD_TYPE_LOCAL_APIC);
    }
}

void smp_init(void)
{
    tty_start_message("SMP initializing");

    smp_detect_cpus();
    cpus = kcalloc(cpuAmount, sizeof(Cpu));

    smp_trampoline_setup();
    smp_startup(cpus);
    smp_trampoline_cleanup();

    initialized = true;

    tty_end_message(TTY_MESSAGE_OK);
}

bool smp_initialized()
{
    return initialized;
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
        if (cpus[id].localApicId == localApicId)
        {
            return &cpus[id];
        }
    }

    debug_panic("Unable to find cpu");
}

void smp_put(void)
{
    interrupts_enable();
}