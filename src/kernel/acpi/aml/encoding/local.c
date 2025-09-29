#include "local.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "expression.h"

uint64_t aml_local_obj_read(aml_state_t* state, aml_node_t** out)
{
    aml_value_t localOp;
    if (aml_value_read(state, &localOp) == ERR)
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

    *out = &state->locals[index];
    return 0;
}
