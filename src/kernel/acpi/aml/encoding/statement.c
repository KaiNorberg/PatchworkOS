#include "statement.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_value.h"
#include "package_length.h"
#include "term.h"

#include <errno.h>

uint64_t aml_predicate_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out)
{
    if (aml_term_arg_read_integer(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg for Predicate");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_else_read(aml_state_t* state, aml_node_t* node, bool shouldExecute)
{
    aml_value_t elseOp;
    if (aml_value_read_no_ext(state, &elseOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ElseOp");
        return ERR;
    }

    if (elseOp.num != AML_ELSE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ElseOp '0x%x'", elseOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLength");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    if (shouldExecute)
    {
        // Execute the TermList
        if (aml_term_list_read(state, node, end) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read TermList");
            return ERR;
        }
    }
    else
    {
        // Skip the TermList
        uint64_t offset = end - state->current;
        aml_state_advance(state, offset);
    }

    return 0;
}

uint64_t aml_def_if_else_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t ifOp;
    if (aml_value_read_no_ext(state, &ifOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IfOp");
        return ERR;
    }

    if (ifOp.num != AML_IF_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid IfOp '0x%x'", ifOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLength");
        return ERR;
    }

    // The end of the If statement, the "Else" part is not included in this length, see section 5.4.19 figure 5.17 of
    // the ACPI spec.
    const uint8_t* end = start + pkgLength;

    aml_qword_data_t predicate;
    if (aml_predicate_read(state, node, &predicate) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Predicate");
        return ERR;
    }

    bool isTrue = predicate != 0;
    if (predicate != 0)
    {
        // Execute the TermList
        if (aml_term_list_read(state, node, end) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read TermList");
            return ERR;
        }
    }
    else
    {
        // Skip the TermList
        uint64_t offset = end - state->current;
        aml_state_advance(state, offset);
    }

    aml_value_t elseOp;
    if (aml_value_peek_no_ext(state, &elseOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek ElseOp");
        return ERR;
    }

    if (elseOp.num == AML_ELSE_OP) // Optional
    {
        return aml_def_else_read(state, node, !isTrue);
    }

    return 0;
}

uint64_t aml_def_noop_read(aml_state_t* state)
{
    aml_value_t noopOp;
    if (aml_value_read_no_ext(state, &noopOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NoopOp");
        return ERR;
    }

    if (noopOp.num != AML_NOOP_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid NoopOp '0x%x'", noopOp.num);
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_statement_opcode_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t op;
    if (aml_value_peek_no_ext(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek op");
        return ERR;
    }

    uint64_t result = 0;
    switch (op.num)
    {
    case AML_IF_OP:
        result = aml_def_if_else_read(state, node);
        break;
    case AML_NOOP_OP:
        result = aml_def_noop_read(state);
        break;
    default:
        AML_DEBUG_ERROR(state, "Unknown statement opcode '0x%x'", op.num);
        errno = ENOSYS;
        return ERR;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read opcode '0x%x'", op.num);
        return ERR;
    }

    return 0;
}
