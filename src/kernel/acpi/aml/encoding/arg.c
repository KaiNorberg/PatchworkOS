#include "arg.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_token.h"
#include "expression.h"

uint64_t aml_arg_obj_read(aml_state_t* state, aml_object_t** out)
{
    aml_token_t argOp;
    if (aml_token_read(state, &argOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ArgOp");
        return ERR;
    }

    if (argOp.num < AML_ARG0_OP || argOp.num > AML_ARG6_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ArgOp %d", argOp.num);
        return ERR;
    }

    uint64_t index = argOp.num - AML_ARG0_OP;

    if (state->args == NULL)
    {
        AML_DEBUG_ERROR(state, "No arguments provided, but Arg%dOp requested", index);
        return ERR;
    }

    if (index >= state->args->count)
    {
        AML_DEBUG_ERROR(state, "Argument index %d out of bounds %d", index, state->args->count);
        return ERR;
    }

    if (state->args->args[index]->type == AML_DATA_OBJECT_REFERENCE)
    {
        aml_object_t* target = state->args->args[index]->objectReference.target;
        if (target == NULL)
        {
            AML_DEBUG_ERROR(state, "Arg%d is an ObjectReference to NULL", index);
            return ERR;
        }

        *out = target;
        return 0;
    }

    *out = state->args->args[index];
    return 0;
}
