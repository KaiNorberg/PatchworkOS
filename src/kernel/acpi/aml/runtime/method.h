#pragma once

#include "acpi/aml/aml_node.h"

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
 * This function evaluates a method node with the provided arguments and stores the return value in the specified
 * returnValue node.
 *
 * @param method Pointer to the method node to evaluate.
 * @param args Pointer to the list of arguments to pass to the method.
 * @param out Pointer to the node where the return value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_method_evaluate(aml_node_t* method, aml_term_arg_list_t* args, aml_node_t* out);

/** @} */
