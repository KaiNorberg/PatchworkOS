#include "trampoline.h"

#include <kernel/acpi/tables.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/drivers/apic.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>

#include <kernel/cpu/regs.h>
#include <kernel/defs.h>

#include <stdint.h>

/**
 * @brief Symmetric Multiprocessing Others Initialization
 * @defgroup modules_smp Symmetric Multiprocessing
 * @ingroup modules
 *
 * Initializes all other CPUs in the system.
 *
 * This module will panic if it, at any point, fails. This is because error recovery during CPU initialization is way
 * outside the scope of my patience.
 *
 * @{
 */

/**
 * @brief Initialize all other CPUs in the system.
 */
void smp_others_init(void)
{
    interrupt_disable();

    trampoline_init();

    cpu_t* bootstrapCpu = cpu_get_unsafe();
    assert(bootstrapCpu->id == CPU_ID_BOOTSTRAP);
    LOG_INFO("bootstrap cpu already started\n");

    madt_t* madt = (madt_t*)acpi_tables_lookup(MADT_SIGNATURE, 0);
    if (madt == NULL)
    {
        panic(NULL, "MADT table not found");
    }

    processor_local_apic_t* lapic;
    MADT_FOR_EACH(madt, lapic)
    {
        if (lapic->header.type != INTERRUPT_CONTROLLER_PROCESSOR_LOCAL_APIC || bootstrapCpu->lapicId == lapic->apicId)
        {
            continue;
        }

        if (!(lapic->flags & PROCESSOR_LOCAL_APIC_ENABLED))
        {
            continue;
        }

        cpu_t* cpu = vmm_alloc(NULL, NULL, sizeof(cpu_t), PML_WRITE | PML_PRESENT | PML_GLOBAL, VMM_ALLOC_NONE);
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

    interrupt_enable();
}

/** @} */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        smp_others_init();
        break;
    case MODULE_EVENT_UNLOAD:
        // Not supported.
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("SMP", "Kai Norberg", "Symmetric Multiprocessing support", OS_VERSION, "MIT", "LOAD_ON_BOOT");