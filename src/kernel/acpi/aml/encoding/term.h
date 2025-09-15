#pragma once

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_state.h"
#include "data.h"

#include <stdint.h>

/**
 * @brief ACPI AML Term Objects Encoding
 * @defgroup kernel_acpi_aml_term Term Objects
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.5 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief ACPI AML TermArg structure
 * @struct aml_term_arg_t
 *
 * A TermArg structure is used to pass certain arguments to opcodes. They dont just store static information, instead
 * they are evaluated at runtime. Think of how in C you can do `myfunc(1, myotherfunc(), 2)`, in this case the
 * `myotherfunc()` argument would be a TermArg in AML.
 */
typedef aml_data_object_t aml_term_arg_t;

/**
 * @brief Reads an TermArg structure from the AML byte stream.
 *
 * A TermArg is defined as `TermArg := ExpressionOpcode | DataObject | ArgObj | LocalObj`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param out The output buffer to store the result of the TermArg.
 * @param expectedType The expected type of the TermArg result, will error if a different type is encountered. If set to
 * `AML_DATA_NONE`, no type checking is performed.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_read(aml_state_t* state, aml_node_t* node, aml_term_arg_t* out, aml_data_type_t expectedType);

/**
 * @brief Reads an Object structure from the AML byte stream.
 *
 * An Object is defined as `Object := NameSpaceModifierObj | NamedObj`.
 *
 * @param state The AML state.
 * @param node The current AML node, can be `NULL`.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_object_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a TermObj structure from the AML byte stream.
 *
 * A TermObj is defined as `TermObj := Object | StatementOpcode | ExpressionOpcode`.
 *
 * @param state The AML state.
 * @param node The current AML node, can be `NULL`.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_termobj_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a TermList structure from the AML byte stream.
 *
 * A TermList structure is defined as `TermList := Nothing | <termobj termlist>`.
 *
 * @param state The AML state.
 * @param node The current AML node, can be `NULL`.
 * @param end The index at which the termlist ends.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_term_list_read(aml_state_t* state, aml_node_t* node, aml_address_t end);

/** @} */
