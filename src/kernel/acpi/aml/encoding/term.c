#include "term.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "data.h"
#include "named.h"
#include "namespace_modifier.h"

#include <errno.h>
#include <stdint.h>

uint64_t aml_termarg_read(aml_state_t* state, aml_scope_t* scope, aml_termarg_t* out, aml_termarg_type_t expectedType)
{
    if (expectedType == AML_TERMARG_NONE || expectedType >= AML_TERMARG_MAX)
    {
        errno = EINVAL;
        return ERR;
    }

    // TODO: Implement other termarg types

    if (expectedType != AML_TERMARG_INTEGER)
    {
        AML_DEBUG_UNIMPLEMENTED_STRUCTURE("Non-Integer TermArg");
        errno = ENOSYS;
        return ERR;
    }

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

    aml_data_object_t dataObject;
    if (aml_data_object_read(state, &dataObject) == ERR)
    {
        return ERR;
    }

    if (!dataObject.isComputational)
    {
        AML_DEBUG_UNIMPLEMENTED_STRUCTURE("Non-Computational DataObject");
        errno = ENOSYS;
        return ERR;
    }

    if (!AML_COMPUTATIONAL_DATA_IS_INTEGER(dataObject.computational))
    {
        AML_DEBUG_UNIMPLEMENTED_STRUCTURE("Non-Integer Computational DataObject");
        errno = ENOSYS;
        return ERR;
    }

    out->type = AML_TERMARG_INTEGER;
    out->integer = AML_COMPUTATIONAL_DATA_AS_INTEGER(dataObject.computational);
    return 0;
}

uint64_t aml_termarg_read_integer(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    aml_termarg_t termarg;
    if (aml_termarg_read(state, scope, &termarg, AML_TERMARG_INTEGER) == ERR)
    {
        return ERR;
    }

    *out = termarg.integer;
    return 0;
}

uint64_t aml_object_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_NAMESPACE_MODIFIER:
        return aml_namespace_modifier_obj_read(state, scope);
    case AML_VALUE_TYPE_NAMED:
        return aml_named_obj_read(state, scope);
    default:
        AML_DEBUG_UNEXPECTED_VALUE(&value);
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_termobj_read(aml_state_t* state, aml_scope_t* scope)
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
        return aml_object_read(state, scope);
    }
}

uint64_t aml_termlist_read(aml_state_t* state, aml_scope_t* scope, aml_address_t end)
{
    while (end > state->instructionPointer)
    {
        // End of buffer not reached => byte is not nothing => must be a termobj.
        if (aml_termobj_read(state, scope) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}
