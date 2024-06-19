#include "smp.h"

#include "apic.h"
#include "debug.h"
#include "hpet.h"
#include "kernel.h"
#include "madt.h"
#include "regs.h"
#include "splash.h"
#include "trampoline.h"
#include "utils.h"

#include <stdlib.h>
#include <string.h>

static cpu_t* cpus = NULL;
static uint8_t cpuAmount = 0;
static bool cpuReady = false;

static bool initialized = false;

static NOINLINE void smp_detect_cpus(void)
{
    madt_lapic_t* record = madt_first_record(MADT_LAPIC);
    while (record != 0)
    {
        if (record->flags & MADT_LAPIC_INITABLE)
        {
            cpuAmount++;
        }

        record = madt_next_record(record, MADT_LAPIC);
    }
}

static NOINLINE uint64_t cpu_init(cpu_t* cpu, uint8_t id, uint8_t localApicId)
{
    cpu->id = id;
    cpu->localApicId = localApicId;
    cpu->idleStack = malloc(CPU_IDLE_STACK_SIZE);
    tss_init(&cpu->tss);
    scheduler_init(&cpu->scheduler);

    cpuReady = false;

    if (localApicId == lapic_id())
    {
        return 0;
    }

    trampoline_cpu_setup(cpu);

    lapic_send_init(localApicId);
    hpet_sleep(10);
    lapic_send_sipi(localApicId, ((uint64_t)TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

    uint64_t timeout = 1000;
    while (!cpuReady)
    {
        hpet_sleep(1);
        timeout--;
        if (timeout == 0)
        {
            return ERR;
        }
    }

    return 0;
}

static NOINLINE void smp_startup(void)
{
    uint8_t newId = 0;

    madt_lapic_t* record = madt_first_record(MADT_LAPIC);
    while (record != NULL)
    {
        if (record->flags & MADT_LAPIC_INITABLE)
        {
            uint8_t id = newId;
            newId++;

            DEBUG_ASSERT(cpu_init(&cpus[id], id, record->localApicId) != ERR, "ap fail");
        }

        record = madt_next_record(record, MADT_LAPIC);
    }
}

void smp_init(void)
{
    smp_detect_cpus();
    cpus = calloc(cpuAmount, sizeof(cpu_t));

    trampoline_setup();
    smp_startup();
    trampoline_cleanup();

    initialized = true;
}

void smp_entry(void)
{
    space_load(NULL);

    kernel_cpu_init();

    cpuReady = true;
    while (true)
    {
        asm volatile("sti");
        asm volatile("hlt");
    }
}

bool smp_initialized(void)
{
    return initialized;
}

void smp_send_ipi(cpu_t const* cpu, uint8_t ipi)
{
    lapic_send_ipi(cpu->localApicId, IPI_BASE + ipi);
}

void smp_send_ipi_to_others(uint8_t ipi)
{
    cpu_t const* self = smp_self_unsafe();
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

cpu_t* smp_cpu(uint8_t id)
{
    return &cpus[id];
}

cpu_t* smp_self(void)
{
    interrupts_disable();

    return &cpus[msr_read(MSR_CPU_ID)];
}

cpu_t* smp_self_unsafe(void)
{
    if (rflags_read() & RFLAGS_INTERRUPT_ENABLE)
    {
        debug_panic("smp_self_unsafe called with interrupts enabled");
    }

    return &cpus[msr_read(MSR_CPU_ID)];
}

cpu_t* smp_self_brute(void)
{
    if (rflags_read() & RFLAGS_INTERRUPT_ENABLE)
    {
        debug_panic("smp_self_brute called with interrupts enabled");
    }

    uint8_t localApicId = lapic_id();
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
