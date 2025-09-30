#pragma once

#include "acpi/aml/aml_node.h"

#include <stdint.h>

/**
 * @brief Data Type Conversion
 * @defgroup kernel_acpi_aml_convert Convert
 * @ingroup kernel_acpi_aml
 *
 * @see Section 19.3.5 of the ACPI specification for more details.
 * @see Section 19.3.5.7 table 19.6 for the conversion priority order.
 * @see Section 19.3.5.7 table 19.7 for a summary of the conversion rules.
 *
 * @{
 */

/**
 * @brief Converts the data in the source node to a allowed type and stores it in the destination node.
 *
 * @param src Pointer to the source node to convert.
 * @param dest Pointer to the destination node where the converted value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_and_store(aml_node_t* src, aml_node_t* dest, aml_data_type_t allowedTypes);

/**
 * @brief Performs a "Implicit Source Operand Conversion" acording to the rules in section 19.3.5.4 of the ACPI
 * specification.
 *
 * @see Section 19.3.5.4 of the ACPI specification for more details.
 *
 * @param source Pointer to the source node to convert.
 * @param out Pointer to the node where the result will be stored.
 * @param allowedTypes Bitmask of allowed destination types.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_source(aml_node_t* source, aml_node_t* out, aml_data_type_t allowedTypes);

/**
 * @brief Performs a "Implicit Result Object Conversion" acording to the rules in section 19.3.5.5 of the ACPI
 * specification.
 *
 * @see Section 19.3.5.5 of the ACPI specification for more details.
 *
 * @param result Pointer to the result node to convert.
 * @param target Pointer to the target node to store the result in. For convenience this can be `NULL`, in which case
 * this does nothing.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_result(aml_node_t* result, aml_node_t* target);

/** @} */
