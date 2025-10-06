#pragma once

#include <stdint.h>

#include "acpi/aml/aml_object.h"

/**
 * @brief Store
 * @defgroup kernel_acpi_aml_store Store
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Store the value from the source object into the target object.
 *
 * Follows the rules in table 19.8 section 19.3.5.8 for the "Store" operator or any operator with a "Target" operand.
 *
 * Additionally if none of the rules apply and dest is uninitialized we use `aml_copy_data_and_type()` to perform the
 * copy.
 *
 * @see Section 19.3.5.8 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to store from.
 * @param dest Pointer to the destination object to store to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_store(aml_object_t* src, aml_object_t* dest);

/** @} */
