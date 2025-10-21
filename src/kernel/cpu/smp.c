#include "smp.h"

#include "acpi/tables.h"
#include "cpu/cpu.h"
#include "cpu/interrupt.h"
#include "drivers/apic.h"
#include "interrupt.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/vmm.h"
#include "trampoline.h"

#include <common/defs.h>
#include <common/regs.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>

static cpu_t bootstrapCpu ALIGNED(PAGE_SIZE) = {0};

cpu_t* _cpus[CPU_MAX] = {&bootstrapCpu};
uint16_t _cpuAmount = 1; // Start with 1 for the bootstrap CPU

static atomic_uint16_t haltedAmount = ATOMIC_VAR_INIT(0);

void smp_bootstrap_init_early(void)
{
    msr_write(MSR_CPU_ID, CPU_ID_BOOTSTRAP);
}

void smp_bootstrap_init(void)
{
    _cpus[CPU_ID_BOOTSTRAP] = &bootstrapCpu;
    if (cpu_init(&bootstrapCpu, CPU_ID_BOOTSTRAP) == ERR)
    {
        panic(NULL, "Failed to initialize bootstrap cpu");
    }
}

void smp_others_init(void)
{
    trampoline_init();

    bootstrapCpu.lapicId = lapic_self_id();
    LOG_INFO("bootstrap cpu %u with lapicid %u, ready\n", (uint64_t)bootstrapCpu.id, (uint64_t)bootstrapCpu.lapicId);

    madt_t* madt = (madt_t*)acpi_tables_lookup(MADT_SIGNATURE, 0);
    if (madt == NULL)
    {
        // Technically we dont need to panic here we could just assume the system is single cpu but the rest of the os
        // needs the madt anyway so we might as well.
        panic(NULL, "MADT table not found");
    }

    processor_local_apic_t* lapic;
    MADT_FOR_EACH(madt, lapic)
    {
        if (lapic->header.type != INTERRUPT_CONTROLLER_PROCESSOR_LOCAL_APIC || bootstrapCpu.lapicId == lapic->apicId)
        {
            continue;
        }

        if (lapic->flags & PROCESSOR_LOCAL_APIC_ENABLED)
        {
            // We need the _cpus to be page aligned so its stacks are also page aligned.
            cpuid_t newId = _cpuAmount++;
            _cpus[newId] = vmm_alloc(NULL, NULL, sizeof(cpu_t), PML_WRITE | PML_PRESENT | PML_GLOBAL, VMM_ALLOC_FAIL_IF_MAPPED);
            if (_cpus[newId] == NULL)
            {
                panic(NULL, "Failed to allocate memory for cpu %d with lapicid %d", (uint64_t)newId,
                    (uint64_t)lapic->apicId);
            }
            memset(_cpus[newId], 0, sizeof(cpu_t));

            trampoline_send_startup_ipi(_cpus[newId], newId, lapic->apicId);

            if (trampoline_wait_ready(CLOCKS_PER_SEC) == ERR)
            {
                panic(NULL, "Timeout waiting for cpu %d with lapicid %d to start", (uint64_t)newId,
                    (uint64_t)lapic->apicId);
            }
        }
    }

    trampoline_deinit();
}

static void smp_halt_ipi(interrupt_frame_t* frame)
{
    (void)frame; // Unused

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
    for (uint8_t id = 0; id < _cpuAmount; id++)
    {
        if (self->id != id)
        {
            lapic_send_ipi(_cpus[id]->lapicId, INTERRUPT_HALT);
        }
    }
}
