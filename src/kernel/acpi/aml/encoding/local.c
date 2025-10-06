#include "local.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_token.h"
#include "expression.h"

uint64_t aml_local_obj_read(aml_state_t* state, aml_object_t** out)
{
    aml_token_t localOp;
    if (aml_token_read(state, &localOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ArgOp");
        return ERR;
    }

    if (localOp.num < AML_LOCAL0_OP || localOp.num > AML_LOCAL7_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid LocalOp %d", localOp.num);
        return ERR;
    }

    uint64_t index = localOp.num - AML_LOCAL0_OP;

    if (state->locals[index]->type == AML_OBJECT_REFERENCE)
    {
        aml_object_t* target = state->locals[index]->objectReference.target;
        if (target == NULL)
        {
            AML_DEBUG_ERROR(state, "Local%d is an ObjectReference to NULL", index);
            return ERR;
        }

        *out = target;
        return 0;
    }

    *out = state->locals[index];
    return 0;
}
