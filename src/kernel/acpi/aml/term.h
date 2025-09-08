#pragma once

#include "aml_op.h"
#include "aml_state.h"
#include "log/log.h"
#include "namespace_modifier.h"

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
 * @brief Parses an Object structure.
 *
 * An Object is defined as `NameSpaceModifierObj | NamedObj`.
 *
 * @param state The AML state.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_object_parse(aml_state_t* state)
{
    aml_op_t op;
    if (aml_op_read(state, &op, AML_OP_FLAG_NAMESPACE_MODIFIER | AML_OP_FLAG_NAMED) == ERR)
    {
        errno = EILSEQ;
        return ERR;
    }

    if (op.props->flags & AML_OP_FLAG_NAMESPACE_MODIFIER)
    {
        return aml_namespace_modifier_obj_parse(state, &op);
    }
    else if (op.props->flags & AML_OP_FLAG_NAMED)
    {
        // TODO: Implement named object parsing.
        LOG_ERR("Named object parsing not implemented\n");
        errno = ENOTSUP;
        return ERR;
    }

    // This really should not happen as invalid opcodes should be caught in the aml_op_read() == ERR check.
    LOG_ERR("Parser error in aml_object_parse()\n");
    errno = EPROTO;
    return ERR;
}

/**
 * @brief Parses a TermObj structure.
 *
 * A TermObj is defined as `Object | StatementOpcode | ExpressionOpcode`.
 *
 * @param state The AML state.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_termobj_parse(aml_state_t* state)
{
    // Attempt to read a statement or expression opcode, if it fails, then its probably an object. Note that an object
    // is technically also defined using opcodes which can be bit confusing.
    aml_op_t op;
    if (aml_op_read(state, &op, AML_OP_FLAG_STATEMENT | AML_OP_FLAG_EXPRESSION) == ERR)
    {
        return aml_object_parse(state);
    }

    // TODO: This stuff.
    /*if (opcode.props->flags & AML_OPCODE_FLAG_STATEMENT)
    {
        return aml_statement_opcode_parse(state);
    }

    if (opcode.props->flags & AML_OPCODE_FLAG_EXPRESSION)
    {
        return aml_expression_opcode_parse(state);
    }*/

    errno = EILSEQ;
    return ERR;
}

/**
 * @brief Parses a TermList structure.
 *
 * A TermList structure is defined as `Nothing | <termobj termlist>`.
 *
 * @param state The AML state.
 * @param limit The index at which the termlist ends.
 * @return On success, 0. On error, `ERR` and `errno` is set.
 */
static inline uint64_t aml_termlist_parse(aml_state_t* state, uint64_t end)
{
    while (end > state->instructionPointer)
    {
        // End of buffer not reached => byte is not nothing => must be a termobj.
        if (aml_termobj_parse(state) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

/** @} */
