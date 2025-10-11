#pragma once

#include "acpi/aml/object.h"

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
 * However, since ACPICA is the standard and they allow for "implicit returns" such that the last evaluated expression
 * is returned if there is no explicit Return statement, we just do what they do. Thus the return value will be set to
 * a a copy of the last evaluated expression or a explicitly returned object.
 *
 * If a method returns without implicitly or explicitly returning a value, a Integer object of value 0 with the
 * `AML_OBJECT_EXCEPTION_ON_USE` flag set is returned.
 *
 * Methods return a copy of the result not a reference to the actual object. See section 19.6.120.
 *
 * @see Section 5.2 of the ACPICA reference for more details.
 * @see Section 19.6.85 of the ACPI specification for more details.
 *
 * @param method Pointer to the method to evaluate.
 * @param argCount Number of arguments provided.
 * @param args Array of pointers to the argument objects, can be `NULL` if `argCount` is 0.
 * @return On success, the return value of the method. On failure, `ERR` and `errno` is set.
 */
aml_object_t* aml_method_evaluate(aml_method_obj_t* method, aml_object_t** args, uint64_t argCount);

/**
 * @brief Wrapper around aml_method_evaluate for zero argument methods that return an integer or nothing and for integer
 * objects.
 *
 * @param method Pointer to the method or integer object to evaluate.
 * @param out Pointer to the variable where the integer return value will be stored, can be `NULL`.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_evaluate_integer(aml_object_t* object, aml_integer_t* out);

/** @} */
