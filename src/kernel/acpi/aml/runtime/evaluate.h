#pragma once

#include "acpi/aml/aml.h"

#include <stdint.h>

/**
 * @brief Evaluate ACPI Nodes
 * @defgroup kernel_acpi_aml_evaluate Evaluate
 * @ingroup kernel_acpi_aml
 *
 * This module provides functionality for evaluating ACPI nodes.
 *
 * @{
 */

/**
 * @brief Evaluate a node and retrieve the result.
 *
 * This functions behaviour depends on the node type, for example, if the node is a method it will execute the method
 * and retrieve the result, if the node is a field it will retrieve the value stored in the field, etc.
 *
 * Note that argCount should always be zero for non method nodes, and if it is not zero an error will be returned.
 *
 * @param node The node to evaluate.
 * @param out Pointer to the buffer where the result of the evaluation will be stored.
 * @param args Pointer to the argument list, can be `NULL` if no arguments are to be passed.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_evaluate(aml_node_t* node, aml_data_object_t* out, aml_term_arg_list_t* args);

/** @} */
