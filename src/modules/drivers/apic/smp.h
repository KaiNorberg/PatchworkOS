#pragma once

#include <kernel/cpu/cpu.h>

/**
 * @brief Symmetric Multiprocessing support via APIC.
 * @defgroup modules_drivers_apic_smp SMP
 * @ingroup modules_drivers_apic
 *
 * Symmetric Multiprocessing (SMP) support is implemented using the Advanced Programmable Interrupt Controller (APIC)
 * system.
 *
 * @{
 */

/**
 * @brief Start all other CPUs in the system.
 *
 * @return On success, `0`. On failure, `ERR`.
 */
uint64_t smp_start_others(void);

/** @} */