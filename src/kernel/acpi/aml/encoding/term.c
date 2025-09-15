#include "term.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "data.h"
#include "named.h"
#include "namespace_modifier.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_term_arg_read(aml_state_t* state, aml_node_t* node, aml_term_arg_t* out, aml_data_type_t expectedType)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_EXPRESSION:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    case AML_VALUE_TYPE_ARG:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    case AML_VALUE_TYPE_LOCAL:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    default:
        break;
    }

    if (aml_data_object_read(state, out) == ERR)
    {
        return ERR;
    }

    if (expectedType != AML_DATA_NONE && out->type != expectedType)
    {
        AML_DEBUG_INVALID_STRUCTURE("TermArg: Expected type does not match actual type");
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_object_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_NAMESPACE_MODIFIER:
        return aml_namespace_modifier_obj_read(state, node);
    case AML_VALUE_TYPE_NAMED:
        return aml_named_obj_read(state, node);
    default:
        AML_DEBUG_UNEXPECTED_VALUE(&value);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_termobj_read(aml_state_t* state, aml_node_t* node)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_STATEMENT:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    case AML_VALUE_TYPE_EXPRESSION:
        AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
        errno = ENOSYS;
        return ERR;
    default:
        return aml_object_read(state, node);
    }
}

uint64_t aml_termlist_read(aml_state_t* state, aml_node_t* node, aml_address_t end)
{
    while (end > state->pos)
    {
        // End of buffer not reached => byte is not nothing => must be a termobj.
        if (aml_termobj_read(state, node) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}
