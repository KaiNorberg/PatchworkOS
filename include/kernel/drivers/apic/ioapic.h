#pragma once

#include <stdint.h>
#include <sys/defs.h>
#include <sys/status.h>

/**
 * @brief Input / Output Advanced Programmable Interrupt Controller.
 * @defgroup kernel_drivers_apic_ioapic IO APIC
 * @ingroup kernel_drivers_apic
 *
 * The IO APICs are used to route external interrupts to a CPUs local APIC. Each IO APIC handles a range of Global
 * System Interrupts (GSIs) or in PatchworkOS terms, physical IRQs, which it receives from external devices such as a
 * keyboard. The IO APIC then routes these physical IRQs to a local APIC using that local APICs ID, that local APIC then
 * triggers the interrupt on its CPU.
 *
 * So, for example, say we have two IO APICs, 0 and 1, where IO APIC 0 handles physical IRQs 0-23 and IO APIC 1 handles
 * physical IRQs 24-47. Then lets say we want to route physical IRQ 1 to CPU 4. In this case, we would use IO APIC 0 to
 * route physical IRQ 1 to the local APIC ID of CPU 4, lets say this ID is 5. The IO APIC would then send the interrupt
 * to the local APIC with ID 5, which would then trigger the interrupt on CPU 4.
 *
 * The range that each IO APIC handles is defined as the range `[globalSystemInterruptBase, globalSystemInterruptBase +
 * maxRedirs)`, where `globalSystemInterruptBase` is defined in the ACPI MADT table and `maxRedirs` is read from the IO
 * APICs version register.
 *
 * @note The only reason there can be multiple IO APICs is for hardware implementation reasons, things we dont care
 * about. As far as I know, the OS itself does not benefit from having multiple IO APICs.
 *
 * @see [ACPI Specification Version 6.6](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 * @see [82093AA I/O ADVANCED PROGRAMMABLE INTERRUPT CONTROLLER
 * (IOAPIC)](https://web.archive.org/web/20161130153145/http://download.intel.com/design/chipsets/datashts/29056601.pdf)
 *
 * @{
 */

/**
 * @brief IO APIC Global System Interrupt type.
 */
typedef uint32_t ioapic_gsi_t;

/**
 * @brief IO APIC Memory Mapped Registers.
 * @enum ioapic_mmio_register_t
 */
typedef enum
{
    IOAPIC_MMIO_REG_SELECT = 0x00,
    IOAPIC_MMIO_REG_DATA = 0x10
} ioapic_mmio_register_t;

/**
 * @brief IO APIC Registers.
 * @enum ioapic_register_t
 */
typedef enum
{
    IOAPIC_REG_IDENTIFICATION = 0x00,
    IOAPIC_REG_VERSION = 0x01,
    IOAPIC_REG_ARBITRATION = 0x02,
    IOAPIC_REG_REDIRECTION_BASE = 0x10
} ioapic_register_t;

/**
 * @brief IO APIC Delivery Modes.
 * @enum ioapic_delivery_mode_t
 */
typedef enum
{
    IOAPIC_DELIVERY_NORMAL = 0,
    IOAPIC_DELIVERY_LOW_PRIO = 1,
    IOAPIC_DELIVERY_SMI = 2,
    IOAPIC_DELIVERY_NMI = 4,
    IOAPIC_DELIVERY_INIT = 5,
    IOAPIC_DELIVERY_EXTERNAL = 7
} ioapic_delivery_mode_t;

/**
 * @brief IO APIC Destination Modes.
 * @enum ioapic_destination_mode_t
 */
typedef enum
{
    IOAPIC_DESTINATION_PHYSICAL = 0,
    IOAPIC_DESTINATION_LOGICAL = 1
} ioapic_destination_mode_t;

/**
 * @brief IO APIC Trigger Modes.
 * @enum ioapic_trigger_mode_t
 */
typedef enum
{
    IOAPIC_TRIGGER_EDGE = 0,
    IOAPIC_TRIGGER_LEVEL = 1
} ioapic_trigger_mode_t;

/**
 * @brief IO APIC Polarity Modes.
 * @enum ioapic_polarity_t
 */
typedef enum
{
    IOAPIC_POLARITY_HIGH = 0,
    IOAPIC_POLARITY_LOW = 1
} ioapic_polarity_t;

/**
 * @brief IO APIC Version Structure.
 * @struct ioapic_version_t
 *
 * Stored in the `IOAPIC_REG_VERSION` register.
 */
typedef struct PACKED
{
    union {
        uint32_t raw;
        struct PACKED
        {
            uint8_t version;
            uint8_t reserved;
            uint8_t maxRedirs;
            uint8_t reserved2;
        };
    };
} ioapic_version_t;

/**
 * @brief IO APIC Redirection Entry Structure.
 * @struct ioapic_redirect_entry_t
 *
 * Represents a single redirection entry in the IO APIC.
 */
typedef union {
    struct PACKED
    {
        uint8_t vector;
        uint8_t deliveryMode : 3;    ///< ioapic_delivery_mode_t
        uint8_t destinationMode : 1; ///< ioapic_destination_mode_t
        uint8_t deliveryStatus : 1;
        uint8_t polarity : 1; ///< ioapic_polarity_t
        uint8_t remoteIRR : 1;
        uint8_t triggerMode : 1; ///< ioapic_trigger_mode_t
        uint8_t mask : 1;        ///< If set, the interrupt is masked (disabled)
        uint64_t reserved : 39;
        uint8_t destination : 8;
    };
    struct PACKED
    {
        uint32_t low;
        uint32_t high;
    } raw;
} ioapic_redirect_entry_t;

/**
 * @brief Initialize all IO APICs found in the system.
 *
 * @return An appropriate status value.
 */
status_t ioapic_all_init(void);

/** @} */