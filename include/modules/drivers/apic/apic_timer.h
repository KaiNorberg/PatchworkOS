#pragma once

#include <stdint.h>

/**
 * @brief Advanced Programmable Interrupt Controller Timer.
 * @defgroup modules_drivers_apic_timer APIC Timer
 * @ingroup modules_drivers_apic
 *
 * Each local APIC is associated with a timer which can be used to generate interrupts at specific intervals, or as we
 * use it, to generate a single interrupt after a specified time.
 *
 * @see [ACPI Specification Version 6.6](https://uefi.org/sites/default/files/resources/ACPI_Spec_6.6.pdf)
 *
 * @{
 */

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
 * @brief Initialize the APIC timer.
 *
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t apic_timer_init(void);

/** @} */