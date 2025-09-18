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
 * @brief Get the access size in bits for a field.
 *
 * The access size is usually determined by the field's access type, for example if the type is `AML_ACCESS_TYPE_BYTE`
 * then the access size is just 8 bits, etc.
 *
 * However, if the access type is `AML_ACCESS_TYPE_ANY` then the behaviour is less clear, see the comments in the
 * function implementation for more details.
 *
 * @param bitSize The bit size of the field.
 * @param accessType The access type of the field.
 * @param regionSpace The region space of the opregion the field belongs to.
 * @param out Pointer to the buffer where the access size will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_get_access_size(aml_bit_size_t bitSize, aml_access_type_t accessType, aml_region_space_t regionSpace,
    aml_bit_size_t* out);

/**
 * @brief Align a bit size down to the nearest access type boundary.
 *
 * For example, if the bits is 20 and accessSize is 8, then the aligned size will be 16 and the remainder will be 4.
 *
 * @param bits The bit size to align.
 * @param accessType The access type to align to.
 * @param out Pointer to the buffer where the aligned size will be stored.
 * @param remainder Pointer to the buffer where the remainder will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_align_bits(aml_bit_size_t bits, aml_bit_size_t accessSize, aml_bit_size_t* out, aml_bit_size_t* remainder);

/** @} */
