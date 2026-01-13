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

/** @} */
