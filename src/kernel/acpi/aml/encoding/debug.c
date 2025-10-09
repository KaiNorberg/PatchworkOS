#include "debug.h"

#include "acpi/aml/aml_token.h"

aml_object_t* aml_debug_obj_read(aml_state_t* state)
{
    if (aml_token_expect(state, AML_DEBUG_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DebugOp");
        return NULL;
    }

    aml_object_t* obj = aml_object_new(state, AML_OBJECT_NONE);
    if (obj == NULL)
    {
        return NULL;
    }

    if (aml_debug_object_set(obj) == ERR)
    {
        DEREF(obj);
        return NULL;
    }

    return obj; // Transfer ownership
}
