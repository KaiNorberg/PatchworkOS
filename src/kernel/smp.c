#include "smp.h"

#include <string.h>

#include "tty.h"
#include "regs.h"
#include "heap.h"
#include "apic.h"
#include "hpet.h"
#include "madt.h"
#include "debug.h"
#include "utils.h"
#include "kernel.h"
#include "trampoline.h"

static Cpu* cpus = NULL;
static uint8_t cpuAmount = 0;
static bool cpuReady = false;

static bool initialized = false;

static NOINLINE void smp_detect_cpus(void)
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

static NOINLINE uint64_t cpu_init(Cpu* cpu, uint8_t id, uint8_t localApicId)
{
    cpu->id = id;
    cpu->localApicId = localApicId;
    cpu->idleStack = kmalloc(CPU_IDLE_STACK_SIZE);
    tss_init(&cpu->tss);
    scheduler_init(&cpu->scheduler);

    cpuReady = false;

    if (localApicId == local_apic_id())
    {
        return 0;
    }

    trampoline_cpu_setup(cpu);

    local_apic_send_init(localApicId);
    hpet_sleep(10);
    local_apic_send_sipi(localApicId, ((uint64_t)TRAMPOLINE_PHYSICAL_START) / PAGE_SIZE);

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

    LocalApicRecord* record = madt_first_record(MADT_RECORD_TYPE_LOCAL_APIC);
    while (record != NULL)
    {
        if (LOCAL_APIC_RECORD_GET_FLAG(record, LOCAL_APIC_RECORD_FLAG_ENABLEABLE))
        {
            uint8_t id = newId;
            newId++;

            if (cpu_init(&cpus[id], id, record->localApicId) == ERR)
            {
                tty_print("CPU ");
                tty_printi(id);
                tty_print(" failed to start!");
                tty_end_message(TTY_MESSAGE_ER);
            }
        }

        record = madt_next_record(record, MADT_RECORD_TYPE_LOCAL_APIC);
    }
}

void smp_init(void)
{
    tty_start_message("SMP initializing");

    smp_detect_cpus();
    cpus = kcalloc(cpuAmount, sizeof(Cpu));

    trampoline_setup();
    smp_startup();
    trampoline_cleanup();

    initialized = true;

    tty_end_message(TTY_MESSAGE_OK);
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

Cpu* smp_cpu(uint8_t id)
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