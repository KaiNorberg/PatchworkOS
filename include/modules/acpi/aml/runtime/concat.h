#pragma once

#include <modules/acpi/aml/object.h>

/**
 * @brief Object Concatenation
 * @defgroup modules_acpi_aml_runtime_concat Concat
 * @ingroup modules_acpi_aml
 */

/**
 * @brief Concatenates two objects according to the rules in section 19.6.12 of the ACPI specification.
 *
 * @param state Pointer to the current AML state.
 * @param source1 Pointer to the first source object to concatenate, can be `AML_UNINITIALIZED`.
 * @param source2 Pointer to the second source object to concatenate. Can be `AML_UNINITIALIZED`.
 * @param result Pointer to the object where the result will be stored.
 * @return On success, `0`. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_concat(aml_state_t* state, aml_object_t* source1, aml_object_t* source2, aml_object_t* result);

/** @} */
