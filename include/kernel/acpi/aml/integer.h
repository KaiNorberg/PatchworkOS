#pragma once

#include <stdint.h>
#include <sys/defs.h>

/**
 * @brief Integer revision handling
 * @defgroup kernel_acpi_aml_integer Integer
 * @ingroup kernel_acpi_aml
 *
 * This module handles the varying size of integers in AML, which can be either 32 or 64 bits depending on the ACPI
 * revision.
 *
 * @{
 */

/**
 * @brief AML Integer type.
 */
typedef uint64_t aml_uint_t;

/**
 * @brief AML Boolean true value.
 */
#define AML_TRUE aml_integer_ones()

/**
 * @brief AML Boolean false value.
 */
#define AML_FALSE 0

/**
 * @brief Initialize integer handling.
 *
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t aml_integer_handling_init(void);

/**
 * @brief Get the byte size of an AML integer.
 *
 * @return The byte size of an AML integer (4 or 8).
 */
uint8_t aml_integer_byte_size(void) PURE;

/**
 * @brief Get the bit size of an AML integer.
 *
 * @return The bit size of an AML integer (32 or 64).
 */
uint8_t aml_integer_bit_size(void) PURE;

/**
 * @brief Get a mask with all bits set for the current AML integer size.
 *
 * @return A mask with all bits set for the current AML integer size.
 */
aml_uint_t aml_integer_ones(void) PURE;

/** @} */
