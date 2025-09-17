#pragma once
#pragma once

#include "acpi/aml/aml.h"

#include <stdint.h>

/**
 * @brief Access Type Handling
 * @defgroup kernel_acpi_aml_access_type Access Type
 * @ingroup kernel_acpi_aml
 *
 * This module provides functionality for handling access types, alignment with access types, etc.
 *
 * @{
 */

/**
 * @brief Align a bit size down to the nearest access type boundary.
 *
 * For example, aligning 10 bits with an accessType of `AML_ACCESS_TYPE_BYTE` will result in 8 bits, with a remainder of
 * 2 bits.
 *
 * @param bits The bit size to align.
 * @param accessType The access type to align to.
 * @param out Pointer to the buffer where the aligned size will be stored.
 * @param remainder Pointer to the buffer where the remainder will be stored.
 */
void aml_align_bits(aml_bit_size_t bits, aml_access_type_t accessType, aml_bit_size_t* out, aml_bit_size_t* remainder);

/** @} */
