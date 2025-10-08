#include "statement.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_token.h"
#include "acpi/aml/runtime/copy.h"
#include "expression.h"
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
    if (aml_token_expect(state, AML_ELSE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ElseOp");
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
        if (aml_term_list_read(state, scope->location, end) == ERR)
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
    if (aml_token_expect(state, AML_IF_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IfOp");
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
        if (aml_term_list_read(state, scope->location, end) == ERR)
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

    aml_token_t elseOp;
    if (aml_token_peek(state, &elseOp) == ERR)
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
    if (aml_token_expect(state, AML_NOOP_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NoopOp");
        return ERR;
    }

    return 0;
}

aml_object_t* aml_arg_object_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_object_t* result = aml_term_arg_read(state, scope, AML_DATA_REF_OBJECTS);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return NULL;
    }

    return result; // Transfer ownership
}

uint64_t aml_def_return_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_RETURN_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ReturnOp");
        return ERR;
    }

    state->flowControl = AML_FLOW_CONTROL_RETURN;

    aml_object_t* argObject = aml_arg_object_read(state, scope);
    if (argObject == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read ArgObject");
        return ERR;
    }
    DEREF_DEFER(argObject);

    if (state->returnValue != NULL)
    {
        if (aml_copy_data_and_type(argObject, state->returnValue) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to copy return value");
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_def_release_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_RELEASE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ReleaseOp");
        return ERR;
    }

    aml_object_t* mutexObject = aml_mutex_object_read(state, scope);
    if (mutexObject == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read MutexObject");
        return ERR;
    }
    DEREF_DEFER(mutexObject);

    assert(mutexObject->type == AML_MUTEX);

    if (aml_mutex_release(&mutexObject->mutex.mutex) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to release mutex");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_break_read(aml_state_t* state)
{
    if (aml_token_expect(state, AML_BREAK_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BreakOp");
        return ERR;
    }

    state->flowControl = AML_FLOW_CONTROL_BREAK;
    return 0;
}

uint64_t aml_def_continue_read(aml_state_t* state)
{
    if (aml_token_expect(state, AML_CONTINUE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ContinueOp");
        return ERR;
    }

    state->flowControl = AML_FLOW_CONTROL_CONTINUE;
    return 0;
}

uint64_t aml_def_while_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_WHILE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read WhileOp");
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

    const uint8_t* loopStart = state->current;
    while (true)
    {
        uint64_t predicate;
        if (aml_predicate_read(state, scope, &predicate) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read Predicate");
            return ERR;
        }

        if (predicate == 0)
        {
            // Advance the state to the end of the while loop
            uint64_t offset = end - state->current;
            aml_state_advance(state, offset);
            return 0;
        }

        // Execute the TermList in the same scope, might change flow control
        if (aml_term_list_read(state, scope->location, end) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read TermList");
            return ERR;
        }

        if (state->flowControl == AML_FLOW_CONTROL_BREAK || state->flowControl == AML_FLOW_CONTROL_RETURN)
        {
            // Advance the state to the end of the while loop
            uint64_t offset = end - state->current;
            aml_state_advance(state, offset);
            state->flowControl = AML_FLOW_CONTROL_EXECUTE;
            return 0;
        }
        else if (state->flowControl == AML_FLOW_CONTROL_CONTINUE)
        {
            state->flowControl = AML_FLOW_CONTROL_EXECUTE;
            // Continue to the next iteration of the while loop
        }
        else if (state->flowControl != AML_FLOW_CONTROL_EXECUTE)
        {
            AML_DEBUG_ERROR(state, "Invalid flow control state in while loop");
            errno = EILSEQ;
            return ERR;
        }

        // Reset the state to the start of the while loop for the next iteration
        state->current = loopStart;
    }
}

uint64_t aml_statement_opcode_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_token_t op;
    if (aml_token_peek(state, &op) == ERR)
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
    case AML_RELEASE_OP:
        result = aml_def_release_read(state, scope);
        break;
    case AML_WHILE_OP:
        result = aml_def_while_read(state, scope);
        break;
    case AML_BREAK_OP:
        result = aml_def_break_read(state);
        break;
    case AML_CONTINUE_OP:
        result = aml_def_continue_read(state);
        break;
    default:
        AML_DEBUG_ERROR(state, "Unknown StatementOpcode '%s' (0x%x)", op.props->name, op.num);
        errno = ENOSYS;
        return ERR;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read StatementOpcode '%s' (0x%x)", op.props->name, op.num);
        return ERR;
    }

    return 0;
}
