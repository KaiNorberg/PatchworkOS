#include <kernel/acpi/tables.h>
#include <kernel/cpu/cpu.h>
#include <kernel/cpu/interrupt.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/module/module.h>

#include <kernel/cpu/regs.h>
#include <libc/defs.h>

#include <stdint.h>

/**
 * @brief Symmetric Multiprocessing support via APIC.
 * @defgroup kernel_drivers_apic_smp SMP
 * @ingroup kernel_drivers_apic
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
void smp_start_others(void);

/** @} */