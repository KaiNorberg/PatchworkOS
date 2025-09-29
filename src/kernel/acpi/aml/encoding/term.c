#include "term.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"

#include "acpi/aml/runtime/evaluate.h"
#include "data.h"
#include "expression.h"
#include "named.h"
#include "namespace_modifier.h"
#include "statement.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_term_arg_read(aml_state_t* state, aml_scope_t* scope, aml_node_t** out, aml_data_type_t allowedTypes)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    aml_node_t* temp = NULL;

    uint64_t result;
    switch (value.props->type)
    {
    case AML_VALUE_TYPE_EXPRESSION:
    case AML_VALUE_TYPE_NAME: // MethodInvocation is a Name
        result = aml_expression_opcode_read(state, scope, &temp);
        break;
    case AML_VALUE_TYPE_ARG:
        result = aml_arg_obj_read(state, &temp);
        break;
    case AML_VALUE_TYPE_LOCAL:
        result = aml_local_obj_read(state, &temp);
        break;
    default:
        result = aml_data_object_read(state, scope, &temp);
        break;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    if (aml_scope_ensure_node(scope, out) == ERR)
    {
        aml_node_deinit(temp);
        return ERR;
    }

    if (aml_evaluate(temp, *out, allowedTypes) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_term_arg_read_integer(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    aml_node_t* temp = NULL;
    if (aml_term_arg_read(state, scope, &temp, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    *out = temp->integer.value;
    return 0;
}

uint64_t aml_object_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_NAMESPACE_MODIFIER:
        return aml_namespace_modifier_obj_read(state, scope);
    case AML_VALUE_TYPE_NAMED:
        return aml_named_obj_read(state, scope);
    default:
        AML_DEBUG_ERROR(state, "Invalid value type: %d", value.props->type);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_term_obj_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_STATEMENT:
        if (aml_statement_opcode_read(state, scope) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read StatementOpcode");
            return ERR;
        }
        return 0;
    case AML_VALUE_TYPE_EXPRESSION:
    {
        aml_node_t* temp = NULL;
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

uint64_t aml_term_list_read(aml_state_t* state, aml_node_t* newLocation, const uint8_t* end)
{
    aml_scope_t scope;
    if (aml_scope_init(&scope, newLocation) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init scope");
        return ERR;
    }

    while (end > state->current && !state->hasHitReturn)
    {
        // End of buffer not reached => byte is not nothing => must be a termobj.
        if (aml_term_obj_read(state, &scope) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read TermObj");
            return ERR;
        }

        aml_scope_reset_temps(&scope);
    }

    return 0;
}
