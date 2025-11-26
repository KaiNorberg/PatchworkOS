#pragma once

#include <kernel/acpi/aml/object.h>

/**
 * @brief Object Runtime Evaluation
 * @defgroup kernel_acpi_aml_evaluate Runtime Evaluation
 * @ingroup kernel_acpi_aml
 *
 * @{
 */

/**
 * @brief Evaluate an AML object.
 *
 * Will attempt to evaluate the given object to the desired target type, by invoking it if its a method and converting
 * the result or just directly converting it.
 *
 * @param state The AML state to use for evaluation, can be `NULL` to use a temporary state.
 * @param object The AML object to evaluate.
 * @param targetTypes A bitmask of desired target types.
 * @return On success, the evaluated AML object. On failure, `NULL` and `errno` is set.
 */
aml_object_t* aml_evaluate(aml_state_t* state, aml_object_t* object, aml_type_t targetTypes);

/** @} */
