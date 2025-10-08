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

uint64_t aml_term_arg_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out, aml_type_t allowedTypes)
{
    aml_token_t op;
    if (aml_token_peek(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    aml_object_t* value = NULL;

    switch (op.props->type)
    {
    case AML_TOKEN_TYPE_EXPRESSION:
    case AML_TOKEN_TYPE_NAME: // MethodInvocation is a Name
        if (aml_expression_opcode_read(state, scope, &value) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ExpressionOpcode");
            return ERR;
        }
        break;
    case AML_TOKEN_TYPE_ARG:
        if (aml_arg_obj_read(state, &value) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ArgObj");
            return ERR;
        }
        break;
    case AML_TOKEN_TYPE_LOCAL:
        if (aml_local_obj_read(state, &value) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read LocalObj");
            return ERR;
        }
        break;
    default:
        value = aml_scope_get_temp(scope);
        if (value == NULL)
        {
            return ERR;
        }

        if (aml_data_object_read(state, scope, value) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read DataObject");
            return ERR;
        }
    }

    aml_type_t valueType = value->type;
    if (valueType & allowedTypes)
    {
        *out = value;
        return 0;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (aml_convert_source(value, *out, allowedTypes) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_term_arg_read_integer(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    aml_object_t* temp = NULL;
    if (aml_term_arg_read(state, scope, &temp, AML_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    assert(temp->type == AML_INTEGER);

    *out = temp->integer.value;
    return 0;
}

uint64_t aml_term_arg_read_string(aml_state_t* state, aml_scope_t* scope, aml_string_t** out)
{
    aml_object_t* temp = NULL;
    if (aml_term_arg_read(state, scope, &temp, AML_STRING) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    assert(temp->type == AML_STRING);

    *out = &temp->string;
    return 0;
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
        aml_object_t* temp = NULL;
        if (aml_expression_opcode_read(state, scope, &temp) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read ExpressionOpcode");
            return ERR;
        }
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

        aml_scope_reset_temps(&scope);
    }

    aml_scope_deinit(&scope);
    return 0;
}
