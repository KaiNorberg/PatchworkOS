#pragma once

#include "data.h"

#include <stdint.h>

typedef struct aml_state aml_state_t;

/**
 * @brief Term Objects Encoding
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
 * @param out The destination node to store the result of the TermArg.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_read(aml_state_t* state, aml_node_t* node, aml_node_t* out);

/**
 * @brief Wrapper around `aml_term_arg_read` that converts the result to an integer.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out The output buffer to store the integer value of the TermArg.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_read_integer(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out);

/**
 * @brief Reads an Object structure from the AML byte stream.
 *
 * An Object is defined as `Object := NameSpaceModifierObj | NamedObj`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a TermObj structure from the AML byte stream.
 *
 * A TermObj is defined as `TermObj := Object | StatementOpcode | ExpressionOpcode`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_obj_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a TermList structure from the AML byte stream.
 *
 * A TermList structure is defined as `TermList := Nothing | <termobj termlist>`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param end Pointer to the end of the TermList.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_list_read(aml_state_t* state, aml_node_t* node, const uint8_t* end);

/** @} */
