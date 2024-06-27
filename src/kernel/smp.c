#include "smp.h"

#include "apic.h"
#include "gdt.h"
#include "hpet.h"
#include "idt.h"
#include "kernel.h"
#include "log.h"
#include "madt.h"
#include "regs.h"
#include "trampoline.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static cpu_t* cpus = NULL;
static uint8_t cpuAmount = 0;
static bool cpuReady = false;

static bool initialized = false;

_Atomic(uint8_t) haltedAmount = ATOMIC_VAR_INIT(0);

static NOINLINE uint64_t cpu_init(cpu_t* cpu, uint8_t id, uint8_t lapicId)
{
    cpu->id = id;
    cpu->lapicId = lapicId;
    cpu->trapDepth = 0;
    cpu->prevFlags = 0;
    cpu->cliAmount = 0;
    tss_init(&cpu->tss);
    sched_context_init(&cpu->schedContext);

    if (lapicId == lapic_id())
    {
        return 0;
    }

    cpuReady = false;

    trampoline_cpu_setup(cpu);

    lapic_send_init(lapicId);
    hpet_sleep(10);
    lapic_send_sipi(lapicId, ((uint64_t)TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

    uint64_t timeout = 10000;
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

static NOINLINE void smp_detect_cpus(void)
{
    madt_lapic_t* record = madt_first_record(MADT_LAPIC);
    while (record != NULL)
    {
        if (record->flags & MADT_LAPIC_INITABLE)
        {
            cpuAmount++;
        }

        record = madt_next_record(record, MADT_LAPIC);
    }

    log_print("SMP: startup, %d cpus detected", (uint64_t)cpuAmount);
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

            LOG_ASSERT(cpu_init(&cpus[id], id, record->lapicId) != ERR, "startup failure");
        }

        record = madt_next_record(record, MADT_LAPIC);
    }
}

void smp_init(void)
{
    smp_detect_cpus();
    cpus = calloc(cpuAmount, sizeof(cpu_t));

    trampoline_init();
    smp_startup();
    trampoline_cleanup();

    initialized = true;
}

void smp_cpu_init(void)
{
    cpu_t* cpu = smp_self_brute();
    msr_write(MSR_CPU_ID, cpu->id);
}

void smp_entry(void)
{
    gdt_load();
    idt_load();

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

void smp_halt_others(void)
{
    smp_send_ipi_to_others(IPI_HALT);

    while (atomic_load(&haltedAmount) < cpuAmount - 1)
    {
        asm volatile("pause");
    }
}

void smp_halt_self(void)
{
    atomic_fetch_add(&haltedAmount, 1);

    while (1)
    {
        asm volatile("cli");
        asm volatile("hlt");
    }
}

void smp_send_ipi(cpu_t const* cpu, uint8_t ipi)
{
    lapic_send_ipi(cpu->lapicId, IPI_BASE + ipi);
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
        log_panic(NULL, "smp_self_unsafe called with interrupts enabled");
    }

    return &cpus[msr_read(MSR_CPU_ID)];
}

cpu_t* smp_self_brute(void)
{
    if (rflags_read() & RFLAGS_INTERRUPT_ENABLE)
    {
        log_panic(NULL, "smp_self_brute called with interrupts enabled");
    }

    uint8_t lapicId = lapic_id();
    for (uint16_t id = 0; id < cpuAmount; id++)
    {
        if (cpus[id].lapicId == lapicId)
        {
            return &cpus[id];
        }
    }

    log_panic(NULL, "Unable to find cpu");
}

void smp_put(void)
{
    interrupts_enable();
}
