#pragma once

#include "acpi/aml/aml.h"
#include "data.h"

#include <stdint.h>

typedef struct aml_state aml_state_t;

/**
 * @brief ACPI AML Term Objects Encoding
 * @defgroup kernel_acpi_aml_term Term Objects
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.5 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Reads an TermArg structure from the AML byte stream.
 *
 * A TermArg is defined as `TermArg := ExpressionOpcode | DataObject | ArgObj | LocalObj`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out The output buffer to store the result of the TermArg.
 * @param expectedType The expected type of the TermArg result, will error if a different type is encountered. If set to
 * `AML_DATA_ANY`, no type checking is performed.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out, aml_data_type_t expectedType);

/**
 * @brief Reads an Object structure from the AML byte stream.
 *
 * An Object is defined as `Object := NameSpaceModifierObj | NamedObj`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_object_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a TermObj structure from the AML byte stream.
 *
 * A TermObj is defined as `TermObj := Object | StatementOpcode | ExpressionOpcode`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_term_obj_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a TermList structure from the AML byte stream.
 *
 * A TermList structure is defined as `TermList := Nothing | <termobj termlist>`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param end The index at which the termlist ends.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_term_list_read(aml_state_t* state, aml_node_t* node, aml_address_t end);

/** @} */
