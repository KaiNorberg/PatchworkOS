#include "trampoline.h"

#include <kernel/acpi/tables.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>

#include <kernel/cpu/regs.h>
#include <sys/defs.h>

#include <stdint.h>

/**
 * @brief Symmetric Multiprocessing support via APIC.
 * @defgroup kernel_smp SMP
 * @ingroup kernel
 *
 * Symmetric Multiprocessing (SMP) support is implemented using the Advanced Programmable Interrupt Controller (APIC)
 * system.
 *
 * SMP initialization will panic if it, at any point, fails. This is because error recovery during CPU initialization is
 * way outside the scope of my patience.
 *
 * @{
 */

/**
 * @brief Starts the other CPUs in the system.
 */
static void smp_start_others(void)
{
    cli_push();

    if (cpu_amount() > 1)
    {
        LOG_INFO("other cpus already started\n");
        cli_pop();
        return;
    }

    trampoline_init();

    assert(SELF->id == CPU_ID_BOOTSTRAP);
    LOG_INFO("bootstrap cpu already started\n");

    madt_t* madt = (madt_t*)acpi_tables_lookup(MADT_SIGNATURE, sizeof(madt_t), 0);
    if (madt == NULL)
    {
        panic(NULL, "MADT table not found");
    }

    processor_local_apic_t* lapic;
    MADT_FOR_EACH(madt, lapic)
    {
        if (lapic->header.type != INTERRUPT_CONTROLLER_PROCESSOR_LOCAL_APIC || _pcpu_lapic->lapicId == lapic->apicId)
        {
            continue;
        }

        if (!(lapic->flags & PROCESSOR_LOCAL_APIC_ENABLED))
        {
            continue;
        }

        cpu_t* cpu = NULL;
        status_t status = vmm_alloc(NULL, (void**)&cpu, sizeof(cpu_t), PAGE_SIZE, PML_WRITE | PML_PRESENT | PML_GLOBAL, VMM_ALLOC_OVERWRITE);
        if (IS_ERR(status))
        {
            panic(NULL, "Failed to allocate memory for cpu with lapicid %d", (uint64_t)lapic->apicId);
        }
        memset(cpu, 0, sizeof(cpu_t));

        LOG_DEBUG("starting cpu with lapicid %d\n", (uint64_t)lapic->apicId);
        trampoline_send_startup_ipi(cpu, lapic->apicId);

        if (!trampoline_wait_ready(CLOCKS_PER_SEC))
        {
            panic(NULL, "Timeout waiting for cpu with lapicid %d to start", (uint64_t)lapic->apicId);
        }
    }

    LOG_INFO("started %u additional cpu(s)\n", cpu_amount() - 1);

    trampoline_deinit();

    cli_pop();
}

/** @} */

status_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_LOAD:
        smp_start_others();
        break;
    default:
        break;
    }

    return OK;
}

MODULE_INFO("SMP Bootstrap", "Kai Norberg", "Symmetric Multiprocessing support via APIC", OS_VERSION, "MIT",
    "BOOT_ALWAYS");