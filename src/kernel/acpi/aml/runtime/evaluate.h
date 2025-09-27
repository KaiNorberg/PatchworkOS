#pragma once

#include "acpi/aml/aml_node.h"

#include <stdint.h>

/**
 * @brief Node Evaluation
 * @defgroup kernel_acpi_aml_evaluate Evaluation
 * @ingroup kernel_acpi_aml
 *
 * This module provides functions to evaluate nodes and convert data types.
 *
 * @see Section 19.3.5 of the ACPI specification for more details.
 * @see Section 19.3.5.7 table 19.7 for a summary of the conversion rules.
 *
 * @{
 */

/**
 * @brief Evaluate a source node and store the "actual data" result in the destination node.
 *
 * @see aml_data_type_flags_t for more information about what "actual data" means.
 *
 * @param src Pointer to the source node to evaluate.
 * @param dest Pointer to the destination node where the result will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_evaluate_to_actual_data(aml_node_t* src, aml_node_t* dest);

/**
 * @brief Evaluate a source node and convert the result to the type of the destination node, then store it there.
 *
 * Will not perform any type conversion.
 *
 * @see Section 19.3.5.5 of the ACPI specification for more details.
 *
 * @param src Pointer to the source node to evaluate.
 * @param dest Pointer to the destination node where the result will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_evaluate_and_store(aml_node_t* src, aml_node_t* dest);

/**
 * @brief Evaluate a source node and convert the result to an integer.
 *
 * @param src Pointer to the source node to evaluate.
 * @param dest Pointer to the destination node where the integer result will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_evaluate_to_integer(aml_node_t* src, aml_node_t* dest);

/** @} */
