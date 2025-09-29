#pragma once

#include <stdint.h>

/**
 * @addtogroup kernel_acpi_aml_data
 *
 * @{
 */

/**
 * @brief The bit width of an AML integer.
 *
 * This is technically decided by the Definition Blocks revision field but we only support revision 2 and up which
 * means 64 bits.
 */
#define AML_INTEGER_BIT_WIDTH 64

/**
 * @brief Represents a size in bits within an opregion.
 */
typedef uint64_t aml_bit_size_t;

/** @} */
