#pragma once

#include <kernel/cpu/cpu.h>
#include <kernel/cpu/irq.h>
#include <kernel/cpu/percpu.h>

#include <stdint.h>

/**
 * @brief Local Advanced Programmable Interrupt Controller.
 * @defgroup kernel_drivers_apic_lapic Local APIC
 * @ingroup kernel_drivers_apic
 *
 * ## Local APICs
 *
 * Each CPU has its own local APIC, which, when used with the IO APICs, allows for more advanced
 * interrupt handling in comparison to the traditional PIC, such as routing interrupts to specific CPUs, interrupt
 * prioritization, and more. Most of its features are not used in PatchworkOS yet.
 *
 * Additionally, the local APICs provide Inter-Processor Interrupts (IPIs) which allow a CPU to interrupt another CPU by
 * using its local APIC.
 *
 * @note Its a common mistake to assume that the local APIC IDs are contiguous, or that they are the same as the CPU
 * IDs, but this is not the case. The local APIC IDs are assigned by the firmware and can be any value.
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
#define LAPIC_REG_ID_OFFSET 24

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
 * @brief Local APIC Structure.
 * @struct lapic_t
 *
 * Represents each CPU's local APIC and local data.
 */
typedef struct
{
    uint64_t ticksPerMs; ///< Initialized to 0, set on first use of the APIC timer on the CPU.
    lapic_id_t lapicId;
} lapic_t;

/**
 * @brief The per-CPU local APIC structure.
 */
extern lapic_t PERCPU _pcpu_lapic;

/**
 * @brief Initialize the local APIC for a CPU.
 *
 * @param cpu The current CPU.
 */
void lapic_init(cpu_t* cpu);

/**
 * @brief Read from a local apic register.
 *
 * @param reg The register to read from.
 * @return The value read from the register.
 */
uint32_t lapic_read(uint32_t reg);

/**
 * @brief Write to a local apic register.
 *
 * @param reg The register to write to.
 * @param value The value to write.
 */
void lapic_write(uint32_t reg, uint32_t value);

/**
 * @brief Send an INIT IPI to the specified local APIC.
 *
 * Sending an INIT IPI will cause the target CPU to enter the INIT state, preparing it for startup.
 *
 * @param id The ID of the local APIC to send the INIT IPI to.
 */
void lapic_send_init(lapic_id_t id);

/**
 * @brief Send a Startup IPI (SIPI) to the specified local APIC.
 *
 * Sending a SIPI will cause the target CPU to start executing code at the specified entry point address.
 *
 * @param id The ID of the local APIC to send the SIPI to.
 * @param entryPoint The entry point address for the SIPI, must be page-aligned.
 */
void lapic_send_sipi(lapic_id_t id, void* entryPoint);

/**
 * @brief Global initialization for the local APICs.
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t lapic_global_init(void);

/** @} */