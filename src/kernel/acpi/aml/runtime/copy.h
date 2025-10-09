#pragma once

#include "acpi/aml/aml_object.h"

#include <stdint.h>

/**
 * @brief Copy
 * @defgroup kernel_acpi_aml_copy Copy
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Copies the data and type from the source object to the destination object, completly overwriting it.
 *
 * @param src Pointer to the source object to copy from.
 * @param dest Pointer to the destination object to copy to, can be of type `AML_UNINITIALIZED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_copy_data_and_type(aml_object_t* src, aml_object_t* dest);

/**
 * @brief Copies the data from the source object to the destination object.
 *
 * Follows the rules in table 19.8 section 19.3.5.8 for the "CopyObject" operator are applied.
 *
 * It will initialize uninitialized objects as specified in section 19.3.5 table 19.5.
 *
 * @see Sections 19.3.5.8 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to copy from.
 * @param dest Pointer to the destination object to copy to, can be of type `AML_UNINITIALIZED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_copy_object(aml_object_t* src, aml_object_t* dest);

/** @} */
