#include "smp.h"

#include "acpi/tables.h"
#include "cpu/vectors.h"
#include "drivers/apic.h"
#include "drivers/hpet.h"
#include "init/init.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sched/thread.h"
#include "trampoline.h"
#include "trap.h"

#include <common/defs.h>
#include <common/regs.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

static cpu_t bootstrapCpu;
static cpu_t* cpus[CPU_MAX];
static uint16_t cpuAmount = 0;
static bool isReady = false;

static atomic_uint16_t haltedAmount = ATOMIC_VAR_INIT(0);

void smp_bootstrap_init(void)
{
    cpuAmount = 1;
    cpus[CPU_BOOTSTRAP_ID] = &bootstrapCpu;
    if (cpu_init(cpus[0], CPU_BOOTSTRAP_ID, 0) == ERR)
    {
        panic(NULL, "Failed to initialize bootstrap cpu");
    }

    msr_write(MSR_CPU_ID, cpus[0]->id);
}

void smp_others_init(void)
{
    trampoline_init();

    cpus[CPU_BOOTSTRAP_ID]->lapicId = lapic_self_id();
    LOG_INFO("bootstrap cpu %u with lapicid %u, ready\n", (uint64_t)cpus[0]->id, (uint64_t)cpus[0]->lapicId);

    madt_t* madt = MADT_GET();

    madt_processor_local_apic_t* lapic;
    MADT_FOR_EACH(madt, lapic)
    {
        if (lapic->header.type != MADT_INTERRUPT_CONTROLLER_PROCESSOR_LOCAL_APIC)
        {
            continue;
        }

        if (cpus[CPU_BOOTSTRAP_ID]->lapicId == lapic->apicId)
        {
            continue;
        }

        if (lapic->flags & MADT_PROCESSOR_LOCAL_APIC_ENABLED)
        {
            cpuid_t newId = cpuAmount++;
            cpus[newId] = heap_alloc(sizeof(cpu_t), HEAP_VMM);
            if (cpus[newId] == NULL)
            {
                panic(NULL, "Failed to allocate memory for cpu %d with lapicid %d", (uint64_t)newId,
                    (uint64_t)lapic->apicId);
            }

            if (cpu_init(cpus[newId], newId, lapic->apicId) == ERR)
            {
                panic(NULL, "Failed to initialize cpu %d with lapicid %d", (uint64_t)newId, (uint64_t)lapic->apicId);
            }

            if (cpu_start(cpus[newId]) == ERR)
            {
                panic(NULL, "Failed to start cpu %d with lapicid %d", (uint64_t)cpus[newId]->id,
                    (uint64_t)cpus[newId]->lapicId);
            }
        }
    }

    trampoline_deinit();
}

static void smp_halt_ipi(trap_frame_t* trapFrame)
{
    (void)trapFrame; // Unused

    atomic_fetch_add(&haltedAmount, 1);

    while (1)
    {
        asm volatile("cli");
        asm volatile("hlt");
    }
}

void smp_halt_others(void)
{
    const cpu_t* self = smp_self_unsafe();
    for (uint8_t id = 0; id < cpuAmount; id++)
    {
        if (self->id != id)
        {
            lapic_send_ipi(cpus[id]->lapicId, VECTOR_HALT);
        }
    }
}

uint16_t smp_cpu_amount(void)
{
    return cpuAmount;
}

cpu_t* smp_cpu(cpuid_t id)
{
    if (id >= cpuAmount)
    {
        panic(NULL, "smp_cpu(): invalid cpu id %u\n", (uint64_t)id);
    }

    return cpus[id];
}

cpu_t* smp_self_unsafe(void)
{
    assert(!(rflags_read() & RFLAGS_INTERRUPT_ENABLE));

    return cpus[msr_read(MSR_CPU_ID)];
}

cpu_t* smp_self(void)
{
    cli_push();

    return cpus[msr_read(MSR_CPU_ID)];
}

void smp_put(void)
{
    cli_pop();
}
