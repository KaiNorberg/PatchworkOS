#pragma once

#include "acpi/aml/aml_object.h"

/**
 * @brief Object Concatenation
 * @defgroup kernel_acpi_aml_runtime_concat Concat
 * @ingroup kernel_acpi_aml
 */

/**
 * @brief Concatenates two objects according to the rules in section 19.6.12 of the ACPI specification.
 *
 * @param source1 Pointer to the first source object to concatenate, can be `AML_UNINITIALIZED`.
 * @param source2 Pointer to the second source object to concatenate. Can be `AML_UNINITIALIZED`.
 * @param result Pointer to the object where the result will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_concat(aml_object_t* source1, aml_object_t* source2, aml_object_t* result);

/** @} */
