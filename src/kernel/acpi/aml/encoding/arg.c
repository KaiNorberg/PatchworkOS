#include "arg.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "expression.h"

uint64_t aml_arg_obj_read(aml_state_t* state, aml_node_t* out)
{
    aml_value_t argOp;
    if (aml_value_read(state, &argOp) == ERR)
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

    if (aml_node_init_object_reference(out, &state->args->args[index]) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to create Arg%dOp reference", index);
        return ERR;
    }

    return 0;
}
