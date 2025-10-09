#include "term.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_token.h"

#include "acpi/aml/runtime/convert.h"
#include "data.h"
#include "expression.h"
#include "named.h"
#include "namespace_modifier.h"
#include "statement.h"

#include <errno.h>
#include <stdint.h>

aml_object_t* aml_term_arg_read(aml_state_t* state, aml_scope_t* scope, aml_type_t allowedTypes)
{
    aml_token_t op;
    if (aml_token_peek(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return NULL;
    }

    aml_object_t* value = NULL;
    switch (op.props->type)
    {
    case AML_TOKEN_TYPE_EXPRESSION:
    case AML_TOKEN_TYPE_NAME: // MethodInvocation is a Name
        value = aml_expression_opcode_read(state, scope);
        break;
    case AML_TOKEN_TYPE_ARG:
        value = aml_arg_obj_read(state);
        break;
    case AML_TOKEN_TYPE_LOCAL:
        value = aml_local_obj_read(state);
        break;
    default:
        value = aml_object_new(state, AML_OBJECT_NONE);
        if (value == NULL)
        {
            return NULL;
        }

        if (aml_data_object_read(state, scope, value) == ERR)
        {
            DEREF(value);
            AML_DEBUG_ERROR(state, "Failed to read DataObject");
            return NULL;
        }
    }

    if (value == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", op.props->name);
        return NULL;
    }

    if (value->type & allowedTypes)
    {
        return value; // Transfer ownership
    }

    aml_object_t* out = aml_object_new(state, AML_OBJECT_NONE);
    if (out == NULL)
    {
        DEREF(value);
        return NULL;
    }

    if (aml_convert_source(value, out, allowedTypes) == ERR)
    {
        DEREF(value);
        DEREF(out);
        return NULL;
    }

    DEREF(value);
    return out; // Transfer ownership
}

uint64_t aml_term_arg_read_integer(aml_state_t* state, aml_scope_t* scope, aml_integer_t* out)
{
    aml_object_t* temp = aml_term_arg_read(state, scope, AML_INTEGER);
    if (temp == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    assert(temp->type == AML_INTEGER);

    *out = temp->integer.value;
    DEREF(temp);
    return 0;
}

aml_string_obj_t* aml_term_arg_read_string(aml_state_t* state, aml_scope_t* scope)
{
    aml_object_t* temp = aml_term_arg_read(state, scope, AML_STRING);
    if (temp == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return NULL;
    }

    assert(temp->type == AML_STRING);

    return &temp->string; // Transfer ownership
}

uint64_t aml_object_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek token");
        return ERR;
    }

    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_NAMESPACE_MODIFIER:
        return aml_namespace_modifier_obj_read(state, scope);
    case AML_TOKEN_TYPE_NAMED:
        return aml_named_obj_read(state, scope);
    default:
        AML_DEBUG_ERROR(state, "Invalid token type: %d", token.props->type);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_term_obj_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_token_t token;
    if (aml_token_peek(state, &token) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek token");
        return ERR;
    }

    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_STATEMENT:
        if (aml_statement_opcode_read(state, scope) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read StatementOpcode");
            return ERR;
        }
        return 0;
    case AML_TOKEN_TYPE_NAME: // MethodInvocation is a Name
    case AML_TOKEN_TYPE_EXPRESSION:
    {
        aml_object_t* temp = aml_expression_opcode_read(state, scope);
        if (temp == NULL)
        {
            AML_DEBUG_ERROR(state, "Failed to read ExpressionOpcode");
            return ERR;
        }
        DEREF(temp);
        return 0;
    }
    default:
        if (aml_object_read(state, scope) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read Object");
            return ERR;
        }
        return 0;
    }
}

uint64_t aml_term_list_read(aml_state_t* state, aml_object_t* newLocation, const uint8_t* end)
{
    aml_scope_t scope;
    if (aml_scope_init(&scope, newLocation) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init scope");
        return ERR;
    }

    while (end > state->current && state->flowControl == AML_FLOW_CONTROL_EXECUTE)
    {
        // End of buffer not reached => byte is not nothing => must be a termobj.
        if (aml_term_obj_read(state, &scope) == ERR)
        {
            aml_scope_deinit(&scope);
            AML_DEBUG_ERROR(state, "Failed to read TermObj");
            return ERR;
        }
    }

    aml_scope_deinit(&scope);
    return 0;
}
