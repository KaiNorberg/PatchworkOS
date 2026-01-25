#include <kernel/drivers/apic/apic_timer.h>
#include <kernel/drivers/apic/ioapic.h>
#include <kernel/drivers/apic/lapic.h>

#include <kernel/cpu/cpu.h>
#include <kernel/init/init.h>
#include <kernel/log/log.h>
#include <kernel/module/module.h>

#include <sys/defs.h>

/**
 * @brief Advanced Programmable Interrupt Controller.
 * @defgroup kernel_drivers_apic APIC
 * @ingroup kernel_drivers
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
        if (lapic_global_init() == _FAIL)
        {
            LOG_ERR("failed to initialize local APICs\n");
            return _FAIL;
        }
        if (apic_timer_init() == _FAIL)
        {
            LOG_ERR("failed to initialize APIC timer\n");
            return _FAIL;
        }
        if (ioapic_all_init() == _FAIL)
        {
            LOG_ERR("failed to initialize IO APICs\n");
            return _FAIL;
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