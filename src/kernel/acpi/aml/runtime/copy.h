#pragma once

#include "acpi/aml/aml_node.h"

#include <stdint.h>

/**
 * @brief Storing and Copying Objects
 * @defgroup kernel_acpi_aml_copy Copy
 * @ingroup kernel_acpi_aml
 *
 * @see Section 19.3.5.8.3 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Creates a deep copy of the content of the source node into the destination node.
 *
 * @param src Pointer to the source node to copy from.
 * @param dest Pointer to the destination node to copy to, will be overwritten.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_copy_raw(aml_node_t* src, aml_node_t* dest);

/**
 * @brief Copies the data from the source node to the destination node.
 *
 * This follows the rules in section 19.3.5.8.3 of the ACPI specification.
 *
 * @param src Pointer to the source node to copy from.
 * @param dest Pointer to the destination node to copy to.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_copy(aml_node_t* src, aml_node_t* dest);

/** @} */
