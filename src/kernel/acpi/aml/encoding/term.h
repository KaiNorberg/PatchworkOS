#pragma once

#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_scope.h"

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
 * @brief Reads an Object structure from the AML byte stream.
 *
 * An Object is defined as `NameSpaceModifierObj | NamedObj`.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_object_read(aml_state_t* state, aml_scope_t* scope);

/**
 * @brief Reads a TermObj structure from the AML byte stream.
 *
 * A TermObj is defined as `Object | StatementOpcode | ExpressionOpcode`.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_termobj_read(aml_state_t* state, aml_scope_t* scope);

/**
 * @brief Reads a TermList structure from the AML byte stream.
 *
 * A TermList structure is defined as `Nothing | <termobj termlist>`.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @param limit The index at which the termlist ends.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
uint64_t aml_termlist_read(aml_state_t* state, aml_scope_t* scope, uint64_t end);

/** @} */
