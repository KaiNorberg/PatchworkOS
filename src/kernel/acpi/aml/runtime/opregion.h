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
 * Genuinely good luck in understanding this code, especially the alignment stuff. Field access is just spaghetti.
 *
 * @{
 */

/**
 * @brief Read the value stored in a Field.
 *
 * This function should not be called manually, instead use `aml_evaluate()` which will call this function when needed.
 *
 * @param field The field to read from.
 * @param out Pointer to the buffer where the result will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_field_read(aml_node_t* field, aml_data_object_t* out);

/**
 * @brief Write a value to a Field.
 *
 * This function should not be called manually, instead use `aml_store()` which will call this function when needed.
 *
 * @param field The field to write to.
 * @param in The data object containing the value to write.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_field_write(aml_node_t* field, aml_data_object_t* in);

/**
 * @brief Read the value stored in an IndexField.
 *
 * This function should not be called manually, instead use `aml_evaluate()` which will call this function when needed.
 *
 * A IndexField works by having two fields, an index field and a data field. The index field is written to with the
 * "selector" or "index" of the data to read, and then the data field is read to get the actual data.
 *
 * @param indexField The IndexField to read from.
 * @param out Pointer to the buffer where the result will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_index_field_read(aml_node_t* indexField, aml_data_object_t* out);

/**
 * @brief Write a value to an IndexField.
 *
 * This function should not be called manually, instead use `aml_store()` which will call this function when needed.
 *
 * @param indexField The IndexField to write to.
 * @param in The data object containing the value to write.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_index_field_write(aml_node_t* indexField, aml_data_object_t* in);

/**
 * @brief Read the value stored in a BankField.
 *
 * This function should not be called manually, instead use `aml_evaluate()` which will call this function when needed.
 *
 * A BankField works similarly to a field, but it has an additional "bank" node which it writes its "BankValue" to
 * (which is like the BankFields id), before performing any access. Think of this like reconfiguring the opregion to a
 * different structure before accessing it.
 *
 * @param bankField The BankField to read from.
 * @param out Pointer to the buffer where the result will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_bank_field_read(aml_node_t* bankField, aml_data_object_t* out);

uint64_t aml_bank_field_write(aml_node_t* bankField, aml_data_object_t* in);

/** @} */
