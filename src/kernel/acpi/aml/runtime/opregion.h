#pragma once
#pragma once

#include "acpi/aml/aml.h"

#include <stdint.h>

/**
 * @brief Opregion and Field Access
 * @defgroup kernel_acpi_aml_opregion Opregion
 * @ingroup kernel_acpi_aml
 *
 * This module provides functionality for accessing Opregions and Fields.
 *
 * Genuinely good luck in understanding this code, especially the alignment stuff.
 *
 * @{
 */

/**
 * @brief Read the value stored in a Field, IndexField, or BankField.
 *
 * This function should not be called manually, instead use `aml_evaluate()` which will call this function when needed.
 *
 * @param field The field to read from.
 * @param out Pointer to the buffer where the result will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_field_read(aml_node_t* field, aml_data_object_t* out);

/** @} */
