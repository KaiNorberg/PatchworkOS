#pragma once

#include "acpi/aml/aml_op.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"

#include <stdint.h>

/**
 * @brief ACPI AML Namespace Modifier Objects Encoding
 * @defgroup kernel_acpi_aml_namespace_modifier Namespace Modifier Objects
 * @ingroup kernel_acpi_aml
 *
 * See section 20.2.5.1 of the ACPI specification for more details.
 *
 * @{
 */

uint64_t aml_def_alias_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op);

uint64_t aml_def_name_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op);

/**
 * @brief Reads a DefScope structure from the AML byte stream.
 *
 * A DefScope structure is defined as `ScopeOp PkgLength NameString TermList`.
 *
 * See section 19.6.122 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @param op The AML op, should have been read by the caller.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
uint64_t aml_def_scope_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op);

/**
 * @brief Reads a NameSpaceModifierObj structure from the AML byte stream.
 *
 * A NameSpaceModifierObj structure is defined as `DefAlias | DefName | DefScope`.
 *
 * @param state The AML state.
 * @param scope The AML scope, can be `NULL`.
 * @param op The AML op, should have been read by the caller.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
uint64_t aml_namespace_modifier_obj_read(aml_state_t* state, aml_scope_t* scope, const aml_op_t* op);

/** @} */
