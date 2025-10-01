#pragma once

#include "acpi/aml/aml_object.h"
#include "data.h"

#include <stdint.h>

typedef struct aml_state aml_state_t;
typedef struct aml_scope aml_scope_t;

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
 * @param scope The current AML scope.
 * @param out Output pointer to the object pointer to store the result, if this points to `NULL`, a temp object will be
 * used.
 * @param allowedTypes Bitmask of allowed types for the TermArg, the result will be evaluated to one of these types.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out, aml_data_type_t allowedTypes);

/**
 * @brief Wrapper around `aml_term_arg_read` that converts the result to an integer.
 *
 * @param state The AML state.
 * @param scope The current AML scope.
 * @param out The output buffer to store the integer value of the TermArg.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_arg_read_integer(aml_state_t* state, aml_scope_t* scope, uint64_t* out);

/**
 * @brief Reads an Object structure from the AML byte stream.
 *
 * An Object is defined as `Object := NameSpaceModifierObj | NamedObj`.
 *
 * @param state The AML state.
 * @param scope The current AML scope.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_object_read(aml_state_t* state, aml_scope_t* scope);

/**
 * @brief Reads a TermObj structure from the AML byte stream.
 *
 * A TermObj is defined as `TermObj := Object | StatementOpcode | ExpressionOpcode`.
 *
 * @param state The AML state.
 * @param scope The current AML scope.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_obj_read(aml_state_t* state, aml_scope_t* scope);

/**
 * @brief Reads a TermList structure from the AML byte stream.
 *
 * A TermList structure is defined as `TermList := Nothing | <termobj termlist>`.
 *
 * Also creates a new scope for reading the TermList.
 *
 * @param state The AML state.
 * @param newLocation The new location of the scope to be created for reading the TermList.
 * @param end Pointer to the end of the TermList.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_term_list_read(aml_state_t* state, aml_object_t* newLocation, const uint8_t* end);

/** @} */
