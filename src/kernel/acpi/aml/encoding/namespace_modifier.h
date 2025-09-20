#pragma once

#include "acpi/aml/aml.h"

#include <sys/list.h>

#include <stdint.h>

typedef struct aml_state aml_state_t;

/**
 * @brief ACPI AML Namespace Modifier Objects Encoding
 * @defgroup kernel_acpi_aml_namespace_modifier Namespace Modifier Objects
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.5.1 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Reads a DefName structure from the AML byte stream.
 *
 * A DefName structure is defined as `DefName := NameOp NameString DataRefObject`.
 *
 * @see Section 19.6.90 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_name_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a DefScope structure from the AML byte stream.
 *
 * A DefScope structure is defined as `DefScope := ScopeOp PkgLength NameString TermList`.
 *
 * @see Section 19.6.122 of the ACPI specification for more details.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_def_scope_read(aml_state_t* state, aml_node_t* node);

/**
 * @brief Reads a NameSpaceModifierObj structure from the AML byte stream.
 *
 * A NameSpaceModifierObj structure is defined as `NameSpaceModifierObj := DefAlias | DefName | DefScope`.
 *
 * @param state The AML state.
 * @param node The current AML node.
 * @param op The AML op, should have been read by the caller.
 * @return On success, 0. On failure, `ERR` and `errno` is set.
 */
uint64_t aml_namespace_modifier_obj_read(aml_state_t* state, aml_node_t* node);

/** @} */
