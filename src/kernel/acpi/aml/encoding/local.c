#include <kernel/acpi/aml/encoding/local.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/expression.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/token.h>

aml_object_t* aml_local_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t localOp;
    aml_token_read(ctx, &localOp);

    if (localOp.num < AML_LOCAL0_OP || localOp.num > AML_LOCAL7_OP)
    {
        AML_DEBUG_ERROR(ctx, "Invalid LocalOp %d", localOp.num);
        return NULL;
    }

    uint64_t index = localOp.num - AML_LOCAL0_OP;
    if (ctx->state->locals[index] == NULL)
    {
        aml_object_t* local = aml_object_new();
        if (local == NULL || aml_local_set(local) == ERR)
        {
            if (local != NULL)
            {
                DEREF(local);
            }
            return NULL;
        }
        ctx->state->locals[index] = &local->local;
        ctx->state->locals[index]->name = AML_NAME('L', 'O', 'C', '0' + index);
    }

    return REF(CONTAINER_OF(ctx->state->locals[index], aml_object_t, local));
}
