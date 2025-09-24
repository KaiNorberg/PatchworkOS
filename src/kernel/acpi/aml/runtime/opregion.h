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
 * @brief Read the value stored in a FieldUnit. FieldUnits include Fields, IndexFields and BankFields.
 *
 * This function should not be called manually, instead use `aml_evaluate()` which will call this function when needed.
 *
 * A IndexField works by having two fields, an index field and a data field. The index field is written to with the
 * "selector" or "index" of the data to read, and then the data field is read to get the actual data.
 *
 * A BankField works similarly to a field, but it has an additional "bank" node which it writes its "BankValue" to
 * (which is like the BankFields id), before performing any access. Think of this like reconfiguring the opregion to a
 * different structure before accessing it.
 *
 * Will acquire the global mutex if the FieldUnits LockRule is set to `AML_LOCK_RULE_LOCK`.
 *
 * @see @ref kernel_acpi_aml_convert
 * @see Section 19.6.48, 19.6.64 and 19.6.7 of the ACPI specification for more details.
 *
 * @param field The field to read from.
 * @param out Pointer to the buffer where the result will be stored.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_field_unit_read(aml_node_t* fieldUnit, aml_node_t* out);

/**
 * @brief Write a value to a FieldUnit. FieldUnits include Fields, IndexFields and BankFields.
 *
 * This function should not be called manually, instead use the `aml_convert*` functions which will call this function
 * when needed.
 *
 * Will acquire the global mutex if the FieldUnits LockRule is set to `AML_LOCK_RULE_LOCK`.
 *
 * @see @ref kernel_acpi_aml_convert
 * @see Section 19.6.48, 19.6.64 and 19.6.7 of the ACPI specification for more details.
 *
 * @param field The field to write to.
 * @param in Pointer to the node containing the value to write.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_field_unit_write(aml_node_t* fieldUnit, aml_node_t* in);

/** @} */
