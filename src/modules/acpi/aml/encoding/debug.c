#include <kernel/acpi/aml/encoding/debug.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/token.h>

status_t aml_debug_obj_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    if (!aml_token_expect(ctx, AML_DEBUG_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DebugOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* obj = aml_object_new();
    if (obj == NULL)
    {
        return ERR(ACPI, NOMEM);
    }

    status_t status = aml_debug_object_set(obj);
    if (IS_ERR(status))
    {
        UNREF(obj);
        return status;
    }

    *out = obj;
    return status;
}
