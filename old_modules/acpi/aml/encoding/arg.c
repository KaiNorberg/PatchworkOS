#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/arg.h>
#include <kernel/acpi/aml/encoding/expression.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/token.h>

#include <sys/status.h>

status_t aml_arg_obj_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_token_t argOp;
    aml_token_read(ctx, &argOp);

    if (argOp.num < AML_ARG0_OP || argOp.num > AML_ARG6_OP)
    {
        AML_DEBUG_ERROR(ctx, "Invalid ArgOp %d", argOp.num);
        return ERR(ACPI, ILSEQ);
    }

    uint64_t index = argOp.num - AML_ARG0_OP;
    if (ctx->state->args[index] == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Arg%d is not set", index);
        return ERR(ACPI, ILSEQ);
    }

    *out = REF(CONTAINER_OF(ctx->state->args[index], aml_object_t, arg));
    return OK;
}
