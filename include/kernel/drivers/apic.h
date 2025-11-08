#pragma once

#include <kernel/acpi/tables.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/defs.h>

#include <stdint.h>

typedef struct cpu cpu_t;

/**
 * @brief Advanced Programmable Interrupt Controller.
 * @ingroup kernel_drivers
 * @defgroup kernel_drivers_apic APIC
 *
 * @see [ACPI Specification Version 6.6](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 *
 * @{
 */

/**
 * @brief Local APIC ID type.
 */
typedef uint8_t lapic_id_t;

/**
 * @brief IO APIC Global System Interrupt type.
 *
 * This identifies a interrupt "globally" across all IO APICs in the system and can be thought of as the "inpur"
 * interrupt that is then routed to a specific CPU's local APIC.
 */
typedef uint32_t ioapic_gsi_t;

/**
 * @brief APIC Timer Modes.
 * @enum apic_timer_mode_t
 */
typedef enum
{
    APIC_TIMER_MASKED = 0x10000, ///< Timer is masked (disabled)
    APIC_TIMER_PERIODIC = 0x20000,
    APIC_TIMER_ONE_SHOT = 0x00000
} apic_timer_mode_t;

/**
 * @brief APIC Timer Divider Values.
 * @enum apic_timer_divider_t
 */
typedef enum
{
    APIC_TIMER_DIV_16 = 0x3,
    APIC_TIMER_DIV_32 = 0x4,
    APIC_TIMER_DIV_64 = 0x5,
    APIC_TIMER_DIV_128 = 0x6,
    APIC_TIMER_DIV_DEFAULT = APIC_TIMER_DIV_16
} apic_timer_divider_t;

/**
 * @brief Local APIC MSR Flags.
 * @enum lapic_msr_flags_t
 */
typedef enum
{
    LAPIC_MSR_ENABLE = 0x800,
    LAPIC_MSR_BSP = 0x100
} lapic_msr_flags_t;

/**
 * @brief Local APIC Registers.
 * @enum lapic_register_t
 */
typedef enum
{
    LAPIC_REG_ID = 0x020,
    LAPIC_REG_VERSION = 0x030,
    LAPIC_REG_TASK_PRIORITY = 0x080,
    LAPIC_REG_ARBITRATION_PRIORITY = 0x090,
    LAPIC_REG_PROCESSOR_PRIORITY = 0x0A0,
    LAPIC_REG_EOI = 0x0B0,
    LAPIC_REG_REMOTE_READ = 0x0C0,
    LAPIC_REG_LOGICAL_DEST = 0x0D0,
    LAPIC_REG_DEST_FORMAT = 0x0E0,
    LAPIC_REG_SPURIOUS = 0x0F0,
    LAPIC_REG_ISR_BASE = 0x100,
    LAPIC_REG_TMR_BASE = 0x180,
    LAPIC_REG_IRR_BASE = 0x200,
    LAPIC_REG_ERROR_STATUS = 0x280,
    LAPIC_REG_LVT_CMCI = 0x2F0,
    LAPIC_REG_ICR0 = 0x300,
    LAPIC_REG_ICR1 = 0x310,
    LAPIC_REG_LVT_TIMER = 0x320,
    LAPIC_REG_LVT_THERMAL = 0x330,
    LAPIC_REG_LVT_PERFCTR = 0x340,
    LAPIC_REG_LVT_LINT0 = 0x350,
    LAPIC_REG_LVT_LINT1 = 0x360,
    LAPIC_REG_LVT_ERROR = 0x370,
    LAPIC_REG_TIMER_INITIAL_COUNT = 0x380,
    LAPIC_REG_TIMER_CURRENT_COUNT = 0x390,
    LAPIC_REG_TIMER_DIVIDER = 0x3E0
} lapic_register_t;

/**
 * @brief The offset at which the lapic id is stored in the LAPIC_REG_ID register.
 */
#define LAPIC_REG_ICR1_ID_OFFSET 24

/**
 * @brief Local APIC Flags.
 * @enum lapic_flags_t
 */
typedef enum
{
    LAPIC_SPURIOUS_ENABLE = (1 << 8),
    LAPIC_LVT_MASKED = (1 << 16)
} lapic_flags_t;

/**
 * @brief Local APIC ICR Delivery Modes.
 * @enum lapic_icr_delivery_mode_t
 */
typedef enum
{
    LAPIC_ICR_FIXED = (0 << 8),
    LAPIC_ICR_LOWEST_PRIORITY = (1 << 8),
    LAPIC_ICR_SMI = (2 << 8),
    LAPIC_ICR_NMI = (4 << 8),
    LAPIC_ICR_INIT = (5 << 8),
    LAPIC_ICR_STARTUP = (6 << 8)
} lapic_icr_delivery_mode_t;

/**
 * @brief Local APIC ICR Flags.
 * @enum lapic_icr_flags_t
 */
typedef enum
{
    LAPIC_ICR_CLEAR_INIT_LEVEL = (1 << 14)
} lapic_icr_flags_t;

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
    IOAPIC_REG_VERSION = 0x01
} ioapic_register_t;

/**
 * @brief Macro to get the redirection entry register for a specific pin.
 *
 * This is used since a redirect entry is 64 bits (a qword/two dwords) and each register is 32 bits (a dword), so each
 * pin uses two registers.
 *
 * @param pin The pin number as in the gsi - the ioapics base gsi.
 * @param high 0 for the low dword, 1 for the high dword.
 * @return The register number.
 */
#define IOAPIC_REG_REDIRECTION(pin, high) (0x10 + (pin) * 2 + (high))

/**
 * @brief APIC Timer Ticks Fixed Point Offset.
 *
 * Used for fixed point arithmetic when returning the apic timer ticks per nanosecond.
 */
#define APIC_TIMER_TICKS_FIXED_POINT_OFFSET 32

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
 * @brief IO APIC Redirection Entry Structure.
 * @struct ioapic_redirect_entry_t
 *
 * Represents a single redirection entry in the IO APIC.
 */
typedef union {
    struct PACKED
    {
        uint8_t vector;
        uint8_t deliveryMode : 3;
        uint8_t destinationMode : 1;
        uint8_t deliveryStatus : 1;
        uint8_t polarity : 1;
        uint8_t remoteIRR : 1;
        uint8_t triggerMode : 1;
        uint8_t mask : 1;
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
 * @brief Configure the apic timer in one-shot mode.
 *
 * Cnfigures the apic timer on the caller cpu to fire a single interrupt after
 * the specified amount of ticks.
 *
 * @param vector The interrupt vector to fire when the timer expires.
 * @param ticks The amount of ticks to wait before firing the interrupt.
 */
void apic_timer_one_shot(interrupt_t vector, uint32_t ticks);

/**
 * @brief Apic timer ticks per nanosecond.
 *
 * Retrieves the ticks that occur every nanosecond in the apic timer on the caller cpu. Due to the fact that this amount
 * of ticks is very small, most likely less then 1, we used fixed point arithmetic to store the result, the offset used
 * for this is `APIC_TIMER_TICKS_FIXED_POINT_OFFSET`.
 *
 * @return The number of ticks per nanosecond, stored using fixed point arithmetic.
 */
uint64_t apic_timer_ticks_per_ns(void);

/**
 * @brief Initialize the local apic for the current cpu.
 */
void lapic_cpu_init(void);

/**
 * @brief Get the lapic id of the current cpu.
 *
 * @return The lapic id of the current cpu.
 */
lapic_id_t lapic_get_id(void);

/**
 * @brief Write to a local apic register.
 *
 * @param reg The register to write to.
 * @param value The value to write.
 */
void lapic_write(lapic_register_t reg, uint32_t value);

/**
 * @brief Read from a local apic register.
 *
 * @param reg The register to read from.
 * @return The value read from the register.
 */
uint32_t lapic_read(lapic_register_t reg);

/**
 * @brief Send an INIT IPI to a local apic.
 *
 * Sending an INIT IPI will cause the target CPU to enter its initialization state which should be done before sending a
 * SIPI.
 *
 * @param id The lapic id to send the INIT IPI to.
 */
void lapic_send_init(lapic_id_t id);

/**
 * @brief Send a Startup IPI to a local apic.
 *
 * Sending a SIPI will cause the target CPU to start executing at the specified entry point, its important to give a
 * small delay after sending an INIT IPI before sending the SIPI to give the hardware time to process the INIT.
 *
 * @param id The lapic id to send the SIPI to.
 * @param entryPoint The entry point to start executing at, must be page aligned.
 */
void lapic_send_sipi(lapic_id_t id, void* entryPoint);

/**
 * @brief Send an Inter-Processor Interrupt (IPI) to a local apic.
 *
 * The effect of sending an IPI is the same as if `asm volatile("int <vector>")` was executed on the target CPU.
 *
 * @param id The lapic id to send the IPI to.
 * @param vector The interrupt vector to send.
 */
void lapic_send_ipi(lapic_id_t id, interrupt_t vector);

/**
 * @brief Send an End Of Interrupt (EOI) signal to the local apic.
 *
 * Must be called after handling an interrupt to notify the local apic that the interrupt has been handled.
 */
void lapic_eoi(void);

/**
 * @brief Read from an IOAPIC register.
 *
 * @param ioapic The IOAPIC to read from.
 * @param reg The register to read.
 * @return The value read from the register.
 */
uint32_t ioapic_read(ioapic_t* ioapic, ioapic_register_t reg);

/**
 * @brief Write to an IOAPIC register.
 *
 * @param ioapic The IOAPIC to write to.
 * @param reg The register to write.
 * @param value The value to write.
 */
void ioapic_write(ioapic_t* ioapic, ioapic_register_t reg, uint32_t value);

/**
 * @brief Get the IOAPIC version.
 *
 * @param ioapic The IOAPIC to get the version for.
 * @return The IOAPIC version.
 */
ioapic_version_t ioapic_get_version(ioapic_t* ioapic);

/**
 * @brief Get the IOAPIC id responsible for a given GSI.
 *
 * @param gsi The GSI to get the IOAPIC id for.
 * @return The IOAPIC responsible for the GSI.
 */
ioapic_t* ioapic_from_gsi(ioapic_gsi_t gsi);

/**
 * @brief Set an IOAPIC redirection entry.
 *
 * When an interrupt is triggered on the given GSI, the IOAPIC will use the redirection entry to determine how and where
 * to send the interrupt.
 *
 * Say we recieve a GSI 1 interrupt (this would usually be a interrupt from the first ps/2 port), and we have a
 * redirection entry which sends it to vector 0x21 (we usually want to avoid using vectors 0x00-0x20 as those are
 * reserved for exceptions) to the CPU with id 0, the IOAPIC will then send an interrupt to CPU 0 with vector 0x21.
 *
 * @param vector The interrupt vector to set.
 * @param gsi The GSI to set the redirection for.
 * @param deliveryMode The delivery mode to use.
 * @param polarity The polarity to use.
 * @param triggerMode The trigger mode to use.
 * @param cpu The target cpu to send the interrupt to.
 * @param enable Whether to enable or disable the redirection.
 */
void ioapic_set_redirect(interrupt_t vector, ioapic_gsi_t gsi, ioapic_delivery_mode_t deliveryMode,
    ioapic_polarity_t polarity, ioapic_trigger_mode_t triggerMode, cpu_t* cpu, bool enable);

/** @} */
