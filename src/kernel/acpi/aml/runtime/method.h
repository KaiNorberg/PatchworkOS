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
 * Return values are a bit more complex then the spec suggests. The spec leaves "implicit returns", as in the method
 * just ends without a Return statement, undefined. In practice this means we can do whatever we want.
 *
 * However since ACPICA is the standard and they allow for "implicit returns" such that the last evaluated expression is
 * returned if there is no explicit Return statement we just do what they do. Thus the `returnValue` will be set to
 * point to a reference of the last evaluated expression, predicate or a explicitly returned object. If a implicit
 * return is performed before any expression is evaluated then an integer object with value 0 is returned.
 *
 * @see Section 5.2 of the ACPICA reference for more details.
 * @see Section 19.6.85 of the ACPI specification for more details.
 *
 * @param method Pointer to the method to evaluate.
 * @param argCount Number of arguments provided.
 * @param args Array of pointers to the argument objects, can be `NULL` if `argCount` is 0.
 * @param returnValue Pointer to the object pointer where the return value will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_evaluate(aml_method_obj_t* method, aml_object_t** args, uint64_t argCount,
    aml_object_t** returnValue);

/**
 * @brief Wrapper around aml_method_evaluate for zero argument methods that return an integer and for integer objects.
 *
 * @param method Pointer to the method or integer object to evaluate.
 * @param out Pointer to the variable where the integer return value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_evaluate_integer(aml_object_t* object, uint64_t* out);

/** @} */
