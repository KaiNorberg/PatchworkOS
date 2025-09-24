#pragma once

#include "aml_node.h"

/**
 * @brief ACPI AML Type Conversion
 * @defgroup kernel_acpi_aml_convert Convert
 * @ingroup kernel_acpi_aml
 *
 * This module provides functions to convert between different ACPI AML data types. It might seem complicated but all
 * this stuff is really just a very, very long list of simple rules.
 *
 * @see Section 19.3.5 of the ACPI specification for more details.
 * @see Section 19.3.5.7 table 19.7 for a summary of the conversion rules.
 *
 * @{
 */

/**
 * @brief Convert data to the appropriate "actual data" type and store it in the destination node.
 *
 * @see aml_data_type_flags_t for more information about what "actual data" means.
 *
 * @param src Pointer to the source node containing the data to convert.
 * @param dest Pointer to the destination node where the converted data will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_to_actual_data(aml_node_t* src, aml_node_t* dest);

/**
 * @brief Convert data to the type of the destination node and store it there.
 *
 * @see Section 19.3.5.5 of the ACPI specification for more details.
 *
 * @param src Pointer to the source node containing the data to convert.
 * @param dest Pointer to the destination node where the converted data will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_and_store(aml_node_t* src, aml_node_t* dest);

/**
 * @brief Convert data to an integer and store it in the destination node.
 *
 * @param src Pointer to the source node containing the data to convert.
 * @param dest Pointer to the destination node where the integer value will be stored.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_convert_to_integer(aml_node_t* src, aml_node_t* dest);

/** @} */
