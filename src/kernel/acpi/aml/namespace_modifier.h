#pragma once

#include "aml_op.h"
#include "aml_state.h"
#include "log/log.h"

#include <errno.h>
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

static inline uint64_t aml_def_alias_parse(aml_state_t* state, const aml_op_t* op)
{
    LOG_ERR("DefAlias not implemented\n");
    errno = ENOTSUP;
    return ERR;
}

static inline uint64_t aml_def_name_parse(aml_state_t* state, const aml_op_t* op)
{
    LOG_ERR("DefName not implemented\n");
    errno = ENOTSUP;
    return ERR;
}

/**
 * @brief Parses a DefScope structure.
 *
 * A DefScope structure is defined as `ScopeOp PkgLength NameString TermList`. Note that `ScopeOp` should have already
 * been read and passed by the caller in `op`.
 *
 * @param state The AML state.
 * @param op The AML op, should have been read by the caller.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_def_scope_parse(aml_state_t* state, const aml_op_t* op)
{

    LOG_ERR("DefScope not implemented\n");
    errno = ENOTSUP;
    return ERR;
}

/**
 * @brief Parses a NameSpaceModifierObj structure.
 *
 * A NameSpaceModifierObj structure is defined as `DefAlias | DefName | DefScope`.
 *
 * @param state The AML state.
 * @param op The AML op, should have been read by the caller.
 * @return uint64_t On success, 0. On failure, `ERR` and `errno` set.
 */
static inline uint64_t aml_namespace_modifier_obj_parse(aml_state_t* state, const aml_op_t* op)
{
    switch (op->num)
    {
    case AML_OP_ALIAS:
        return aml_def_alias_parse(state, op);
    case AML_OP_NAME:
        return aml_def_name_parse(state, op);
    case AML_OP_SCOPE:
        return aml_def_scope_parse(state, op);
    default:
        errno = EILSEQ;
        return ERR;
    }
}

/** @} */
