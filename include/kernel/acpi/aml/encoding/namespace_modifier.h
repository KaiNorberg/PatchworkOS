#pragma once

#include <sys/list.h>

#include <stdint.h>

typedef struct aml_object aml_object_t;
typedef struct aml_term_list_ctx aml_term_list_ctx_t;

/**
 * @brief Namespace Modifier Objects Encoding
 * @defgroup kernel_acpi_aml_encoding_namespace_modifier Namespace Modifier Objects
 * @ingroup kernel_acpi_aml
 *
 * @see Section 20.2.5.1 of the ACPI specification for more details.
 *
 * @{
 */

/**
 * @brief Reads a DefAlias structure from the AML byte stream.
 *
 * A DefAlias structure is defined as `DefAlias := AliasOp NameString NameString`.
 *
 * @see Section 19.6.4 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t aml_def_alias_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefName structure from the AML byte stream.
 *
 * A DefName structure is defined as `DefName := NameOp NameString DataRefObject`.
 *
 * @see Section 19.6.90 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t aml_def_name_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a DefScope structure from the AML byte stream.
 *
 * A DefScope structure is defined as `DefScope := ScopeOp PkgLength NameString TermList`.
 *
 * @see Section 19.6.122 of the ACPI specification for more details.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t aml_def_scope_read(aml_term_list_ctx_t* ctx);

/**
 * @brief Reads a NameSpaceModifierObj structure from the AML byte stream.
 *
 * A NameSpaceModifierObj structure is defined as `NameSpaceModifierObj := DefAlias | DefName | DefScope`.
 *
 * @param ctx The context of the TermList that this structure is part of.
 * @param op The AML op, should have been read by the caller.
 * @return On success, `0`. On failure, `_FAIL` and `errno` is set.
 */
uint64_t aml_namespace_modifier_obj_read(aml_term_list_ctx_t* ctx);

/** @} */
