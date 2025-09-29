#pragma once

#include "acpi/aml/aml_node.h"

#include <stdint.h>

/**
 * @brief Node Evaluation and Conversion
 * @defgroup kernel_acpi_aml_evaluate Evaluation
 * @ingroup kernel_acpi_aml
 *
 * This module provides functions to evaluate nodes and convert data types.
 *
 * @see Section 19.3.5 of the ACPI specification for more details.
 * @see Section 19.3.5.7 table 19.6 for the conversion priority order.
 * @see Section 19.3.5.7 table 19.7 for a summary of the conversion rules.
 *
 * @{
 */

/**
 * @brief Convert a source node to the allowed type with the highest priority and store the result in the destination
 * node.
 *
 * Note that the content of dest does matter, some types will completly overwrite dest, while others might modify it.
 * For instance when evaluating a Integer to a Buffer if dest also happens to be a Buffer then the length of the buffer
 * will remain the same. If dest is not a buffer then its overwritten. Check the spec for details.
 *
 * @param src Pointer to the source node to evaluate.
 * @param dest Pointer to the destination node where the result will be stored.
 * @param allowedTypes Bitmask of allowed types for conversion.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_evaluate(aml_node_t* src, aml_node_t* dest, aml_data_type_t allowedTypes);

/** @} */
