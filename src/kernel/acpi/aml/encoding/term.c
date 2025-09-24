#include "term.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "acpi/aml/aml_convert.h"
#include "data.h"
#include "expression.h"
#include "named.h"
#include "namespace_modifier.h"
#include "statement.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_term_arg_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    uint64_t result;
    switch (value.props->type)
    {
    case AML_VALUE_TYPE_EXPRESSION:
    case AML_VALUE_TYPE_NAME: // MethodInvocation is a Name
        result = aml_expression_opcode_read(state, node, out);
        break;
    case AML_VALUE_TYPE_ARG:
        AML_DEBUG_ERROR(state, "Unsupported value type: ARG");
        errno = ENOSYS;
        result = ERR;
        break;
    case AML_VALUE_TYPE_LOCAL:
        AML_DEBUG_ERROR(state, "Unsupported value type: LOCAL");
        errno = ENOSYS;
        result = ERR;
        break;
    default:
        result = aml_data_object_read(state, node, out);
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg");
        return ERR;
    }

    return result;
}

uint64_t aml_term_arg_read_integer(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out)
{
    aml_node_t temp = AML_NODE_CREATE;
    if (aml_term_arg_read(state, node, &temp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg");
        return ERR;
    }

    aml_node_t integer = AML_NODE_CREATE;
    if (aml_convert_to_integer(&temp, &integer) == ERR)
    {
        aml_node_deinit(&temp);
        AML_DEBUG_ERROR(state, "Failed to convert term arg to integer");
        return ERR;
    }

    assert(integer.type == AML_DATA_INTEGER);

    *out = integer.integer.value;
    aml_node_deinit(&temp);
    aml_node_deinit(&integer);
    return 0;
}

uint64_t aml_object_read(aml_state_t* state, aml_node_t* node)
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
        return aml_namespace_modifier_obj_read(state, node);
    case AML_VALUE_TYPE_NAMED:
        return aml_named_obj_read(state, node);
    default:
        AML_DEBUG_ERROR(state, "Invalid value type: %d", value.props->type);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_term_obj_read(aml_state_t* state, aml_node_t* node)
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
        if (aml_statement_opcode_read(state, node) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read statement opcode");
            return ERR;
        }
        return 0;
    case AML_VALUE_TYPE_EXPRESSION:
    {
        aml_node_t temp = AML_NODE_CREATE;
        if (aml_expression_opcode_read(state, node, &temp) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read expression opcode");
            return ERR;
        }
        aml_node_deinit(&temp);
        return 0;
    }
    default:
        if (aml_object_read(state, node) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read object");
            return ERR;
        }
        return 0;
    }
}

uint64_t aml_term_list_read(aml_state_t* state, aml_node_t* node, aml_address_t end)
{
    while (end > state->pos)
    {
        // End of buffer not reached => byte is not nothing => must be a termobj.
        if (aml_term_obj_read(state, node) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read term obj");
            return ERR;
        }
    }

    return 0;
}
