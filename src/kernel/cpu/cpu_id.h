#pragma once

#include <stdint.h>

/**
 * @addtogroup kernel_cpu
 *
 * @{
 */

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
typedef uint16_t cpuid_t;

/** @} */
