#include "local.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_token.h"
#include "expression.h"

aml_object_t* aml_local_obj_read(aml_state_t* state)
{
    aml_token_t localOp;
    if (aml_token_read(state, &localOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ArgOp");
        return NULL;
    }

    if (localOp.num < AML_LOCAL0_OP || localOp.num > AML_LOCAL7_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid LocalOp %d", localOp.num);
        return NULL;
    }

    uint64_t index = localOp.num - AML_LOCAL0_OP;
    return REF(CONTAINER_OF(state->locals[index], aml_object_t, local));
}
