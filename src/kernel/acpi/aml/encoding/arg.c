#include "arg.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_token.h"
#include "expression.h"

aml_object_t* aml_arg_obj_read(aml_state_t* state)
{
    aml_token_t argOp;
    if (aml_token_read(state, &argOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ArgOp");
        return NULL;
    }

    if (argOp.num < AML_ARG0_OP || argOp.num > AML_ARG6_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ArgOp %d", argOp.num);
        return NULL;
    }

    uint64_t index = argOp.num - AML_ARG0_OP;
    return REF(CONTAINER_OF(state->args[index], aml_object_t, arg));
}
