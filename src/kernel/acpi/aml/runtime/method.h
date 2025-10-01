#pragma once

#include "acpi/aml/aml_object.h"

#include <stdint.h>

/**
 * @brief Method Evaluation
 * @defgroup kernel_acpi_aml_method Methods
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Evaluate a method with the given arguments.
 *
 * This function evaluates a method object with the provided arguments and stores the return value in the specified
 * returnValue object.
 *
 * @see Section 19.6.85 of the ACPI specification for more details.
 *
 * @param method Pointer to the method object to evaluate.
 * @param args Pointer to the list of arguments to pass to the method, can be `NULL`.
 * @param returnValue Pointer to the object where the return value will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_evaluate(aml_object_t* method, aml_term_arg_list_t* args, aml_object_t* returnValue);

/**
 * @brief Wrapper around aml_method_evaluate for zero argument methods that return an integer and for integer objects.
 *
 * @param method Pointer to the method or integer object to evaluate.
 * @param out Pointer to the variable where the integer return value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_evaluate_integer(aml_object_t* object, uint64_t* out);

/** @} */
