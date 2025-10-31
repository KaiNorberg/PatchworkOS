#pragma once

#include <kernel/acpi/aml/object.h>

#include <stdint.h>

/**
 * @brief Method Evaluation
 * @defgroup kernel_acpi_aml_method Methods
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Invoke a method with the given arguments.
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
 * @see aml_overlay_t for more details about the parent overlay.
 *
 * @param parentState The current AML state, this will not be used for anything other than getting the parent overlay.
 * @param method Pointer to the method to invoke.
 * @param args Array of pointers to the argument objects, can be `NULL`, must be null-terminated.
 * @return On success, the return value of the method. On failure, `ERR` and `errno` is set.
 */
aml_object_t* aml_method_invoke(aml_state_t* parentState, aml_method_obj_t* method, aml_object_t** args);

/** @} */
