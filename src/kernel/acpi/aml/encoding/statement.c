#include "statement.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "acpi/aml/runtime/copy.h"
#include "package_length.h"
#include "term.h"

#include <errno.h>

uint64_t aml_predicate_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_else_read(aml_state_t* state, aml_scope_t* scope, bool shouldExecute)
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
        // Execute the TermList in the same scope
        if (aml_term_list_read(state, scope->node, end) == ERR)
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

uint64_t aml_def_if_else_read(aml_state_t* state, aml_scope_t* scope)
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

    uint64_t predicate;
    if (aml_predicate_read(state, scope, &predicate) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Predicate");
        return ERR;
    }

    bool isTrue = predicate != 0;
    if (predicate != 0)
    {
        // Execute the TermList in the same scope
        if (aml_term_list_read(state, scope->node, end) == ERR)
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
        if (aml_def_else_read(state, scope, !isTrue) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ElseOp");
            return ERR;
        }
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

uint64_t aml_arg_object_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out)
{
    if (aml_term_arg_read(state, scope, out, AML_DATA_ALL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_return_read(aml_state_t* state, aml_scope_t* scope)
{
    state->hasHitReturn = true;

    aml_value_t returnOp;
    if (aml_value_read_no_ext(state, &returnOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ReturnOp");
        return ERR;
    }

    if (returnOp.num != AML_RETURN_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ReturnOp '0x%x'", returnOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* argObject = NULL;
    if (aml_arg_object_read(state, scope, &argObject) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ArgObject");
        return ERR;
    }

    if (state->returnValue != NULL)
    {
        if (aml_copy_raw(argObject, state->returnValue) == ERR)
        {
            aml_node_deinit(argObject);
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_statement_opcode_read(aml_state_t* state, aml_scope_t* scope)
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
        result = aml_def_if_else_read(state, scope);
        break;
    case AML_NOOP_OP:
        result = aml_def_noop_read(state);
        break;
    case AML_RETURN_OP:
        result = aml_def_return_read(state, scope);
        break;
    default:
        AML_DEBUG_ERROR(state, "Unknown StatementOpcode '0x%x'", op.num);
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
