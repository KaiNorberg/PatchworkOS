#include <modules/drivers/apic/apic_timer.h>
#include <modules/drivers/apic/ioapic.h>
#include <modules/drivers/apic/lapic.h>

#include <kernel/cpu/cpu.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/module/module.h>

#include <sys/defs.h>

/**
 * @brief Advanced Programmable Interrupt Controller.
 * @defgroup modules_drivers_apic APIC
 * @ingroup modules_drivers
 *
 * This module implements the Advanced Programmable Interrupt Controller (APIC) driver, which includes the per-CPU
 * local APICs, the IO APICs and the APIC timer.
 *
 * @see [ACPI Specification Version 6.6](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 */

uint64_t _module_procedure(const module_event_t* event)
{
    switch (event->type)
    {
    case MODULE_EVENT_DEVICE_ATTACH:
        if (lapic_global_init() == ERR)
        {
            LOG_ERR("failed to initialize local APICs\n");
            return ERR;
        }
        if (apic_timer_init() == ERR)
        {
            LOG_ERR("failed to initialize APIC timer\n");
            return ERR;
        }
        if (ioapic_all_init() == ERR)
        {
            LOG_ERR("failed to initialize IO APICs\n");
            return ERR;
        }
        PERCPU_INIT();
        break;
    case MODULE_EVENT_DEVICE_DETACH:
        PERCPU_DEINIT();
        break;
    default:
        break;
    }

    return 0;
}

MODULE_INFO("APIC Driver", "Kai Norberg", "A driver for the APIC, local APIC and IOAPIC", OS_VERSION, "MIT", "PNP0003");