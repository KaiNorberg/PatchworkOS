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
 * Will initialize uninitialized objects as specified in section 19.3.5 table 19.5.
 *
 * If dest is a debug object, we use `aml_convert()` which will print the value to the console.
 *
 * @see Section 19.3.5.8 of the ACPI specification for more details.
 *
 * @param src Pointer to the source object to store from.
 * @param dest Pointer to the destination object to store to, can be of type `AML_UNINITIALIZED`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_store(aml_object_t* src, aml_object_t* dest);

/** @} */
