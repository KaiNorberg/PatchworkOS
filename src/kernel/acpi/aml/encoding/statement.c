#include "statement.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_value.h"
#include "package_length.h"
#include "term.h"

#include <errno.h>

uint64_t aml_predicate_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    return aml_term_arg_read(state, node, out, AML_DATA_INTEGER);
}

uint64_t aml_def_else_read(aml_state_t* state, aml_node_t* node, bool shouldExecute)
{
    aml_value_t elseOp;
    if (aml_value_read_no_ext(state, &elseOp) == ERR)
    {
        return ERR;
    }

    if (elseOp.num != AML_ELSE_OP)
    {
        AML_DEBUG_INVALID_STRUCTURE("ElseOp");
        errno = EILSEQ;
        return ERR;
    }

    aml_address_t start = state->pos;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    if (shouldExecute)
    {
        // Execute the TermList
        if (aml_term_list_read(state, node, end) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        // Skip the TermList
        aml_address_t offset = end - state->pos;
        aml_state_advance(state, offset);
    }

    return 0;
}

uint64_t aml_def_if_else_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t ifOp;
    if (aml_value_read_no_ext(state, &ifOp) == ERR)
    {
        return ERR;
    }

    if (ifOp.num != AML_IF_OP)
    {
        AML_DEBUG_INVALID_STRUCTURE("IfOp");
        errno = EILSEQ;
        return ERR;
    }

    aml_address_t start = state->pos;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    // The end of the If statement, the "Else" part is not included in this length, see section 5.4.19 figure 5.17 of
    // the ACPI spec.
    aml_address_t end = start + pkgLength;

    aml_data_object_t predicate;
    if (aml_predicate_read(state, node, &predicate) == ERR)
    {
        return ERR;
    }

    bool isTrue = predicate.type == AML_DATA_INTEGER && predicate.integer != 0;
    if (isTrue)
    {
        // Execute the TermList
        if (aml_term_list_read(state, node, end) == ERR)
        {
            aml_data_object_deinit(&predicate);
            return ERR;
        }
    }
    else
    {
        // Skip the TermList
        aml_address_t offset = end - state->pos;
        aml_state_advance(state, offset);
    }

    aml_value_t elseOp;
    if (aml_value_peek_no_ext(state, &elseOp) == ERR)
    {
        return ERR;
    }

    if (elseOp.num == AML_ELSE_OP) // Optional
    {
        return aml_def_else_read(state, node, !isTrue);
    }

    return 0;
}

uint64_t aml_statement_opcode_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t op;
    if (aml_value_peek_no_ext(state, &op) == ERR)
    {
        return ERR;
    }

    switch (op.num)
    {
    case AML_IF_OP:
        return aml_def_if_else_read(state, node);
    default:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&op);
        errno = ENOSYS;
        return ERR;
    }
}
