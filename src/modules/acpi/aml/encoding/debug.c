#include <modules/acpi/aml/encoding/debug.h>

#include <modules/acpi/aml/debug.h>
#include <modules/acpi/aml/token.h>

aml_object_t* aml_debug_obj_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_DEBUG_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DebugOp");
        return NULL;
    }

    aml_object_t* obj = aml_object_new();
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
