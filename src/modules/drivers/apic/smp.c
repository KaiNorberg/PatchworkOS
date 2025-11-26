#include "smp.h"
#include "lapic.h"

#include <kernel/acpi/tables.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>

#include <kernel/cpu/regs.h>
#include <kernel/defs.h>

#include <stdint.h>

uint64_t smp_start_others(void)
{
    return 0;
    /*interrupt_disable();

    if (cpu_amount() > 1)
    {
        LOG_INFO("other cpus already started\n");
        interrupt_enable();
        return;
    }

    trampoline_init();

    cpu_t* bootstrapCpu = cpu_get_unsafe();
    assert(bootstrapCpu->id == CPU_ID_BOOTSTRAP);

    lapic_t* bootstrapLapic = lapic_get(bootstrapCpu->id);
    assert(bootstrapLapic != NULL);

    LOG_INFO("bootstrap cpu already started\n");

    madt_t* madt = (madt_t*)acpi_tables_lookup(MADT_SIGNATURE, sizeof(madt_t), 0);
    if (madt == NULL)
    {
        panic(NULL, "MADT table not found");
    }

    processor_local_apic_t* lapic;
    MADT_FOR_EACH(madt, lapic)
    {
        if (lapic->header.type != INTERRUPT_CONTROLLER_PROCESSOR_LOCAL_APIC || bootstrapLapic->lapicId == lapic->apicId)
        {
            continue;
        }

        if (!(lapic->flags & PROCESSOR_LOCAL_APIC_ENABLED))
        {
            continue;
        }

        cpu_t* cpu = vmm_alloc(NULL, NULL, sizeof(cpu_t), PML_WRITE | PML_PRESENT | PML_GLOBAL, VMM_ALLOC_OVERWRITE);
        if (cpu == NULL)
        {
            panic(NULL, "Failed to allocate memory for cpu with lapicid %d", (uint64_t)lapic->apicId);
        }
        memset(cpu, 0, sizeof(cpu_t));

        LOG_DEBUG("starting cpu with lapicid %d\n", (uint64_t)lapic->apicId);
        trampoline_send_startup_ipi(cpu, lapic->apicId);

        if (trampoline_wait_ready(CLOCKS_PER_SEC) == ERR)
        {
            panic(NULL, "Timeout waiting for cpu with lapicid %d to start", (uint64_t)lapic->apicId);
        }
    }

    LOG_INFO("started %u additional cpu(s)\n", cpu_amount() - 1);

    trampoline_deinit();

    interrupt_enable();*/
}