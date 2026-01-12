#pragma once

#include <kernel/cpu/regs.h>

#include <assert.h>
#include <stdint.h>

/**
 * @addtogroup kernel_cpu
 *
 * @{
 */

/**
 * @brief The offset of the `id` member in the `cpu_t` structure.
 *
 * Needed to access the CPU ID from assembly code.
 */
#define CPU_OFFSET_ID 0x8

/**
 * @brief Maximum number of CPUs supported.
 */
#define CPU_MAX UINT8_MAX

/**
 * @brief ID of the bootstrap CPU.
 */
#define CPU_ID_BOOTSTRAP 0

/**
 * @brief Invalid CPU ID.
 */
#define CPU_ID_INVALID UINT16_MAX

/**
 * @brief Type used to identify a CPU.
 */
typedef uint16_t cpu_id_t;

/**
 * @brief Gets the current CPU ID.
 *
 * @warning This function does not disable interrupts, it should thus only be used when interrupts are already disabled.
 *
 * @return The current CPU ID.
 */
static inline cpu_id_t cpu_id_get(void)
{
    cpu_id_t id;
    asm volatile("movw %%gs:%P1, %0" : "=r"(id) : "i"(CPU_OFFSET_ID));
    return id;
}

/** @} */
