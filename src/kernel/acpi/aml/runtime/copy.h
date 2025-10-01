#pragma once

#include "acpi/aml/aml_object.h"

#include <stdint.h>

/**
 * @brief Storing and Copying Objects
 * @defgroup kernel_acpi_aml_copy Copy
 * @ingroup kernel_acpi_aml
 *
 * @see Section 19.3.5.8.3 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Creates a deep copy of the content of the source object into the destination object.
 *
 * @param src Pointer to the source object to copy from.
 * @param dest Pointer to the destination object to copy to, will be overwritten.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_copy_raw(aml_object_t* src, aml_object_t* dest);

/**
 * @brief Copies the data from the source object to the destination object.
 *
 * This follows the rules in section 19.3.5.8.3 of the ACPI specification.
 *
 * @param src Pointer to the source object to copy from.
 * @param dest Pointer to the destination object to copy to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_copy(aml_object_t* src, aml_object_t* dest);

/** @} */
