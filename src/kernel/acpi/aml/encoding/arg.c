#include <kernel/acpi/aml/encoding/arg.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/expression.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/token.h>

aml_object_t* aml_arg_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t argOp;
    aml_token_read(ctx, &argOp);

    if (argOp.num < AML_ARG0_OP || argOp.num > AML_ARG6_OP)
    {
        AML_DEBUG_ERROR(ctx, "Invalid ArgOp %d", argOp.num);
        return NULL;
    }

    uint64_t index = argOp.num - AML_ARG0_OP;
    if (ctx->state->args[index] == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Arg%d is not set", index);
        errno = EILSEQ;
        return NULL;
    }

    return REF(CONTAINER_OF(ctx->state->args[index], aml_object_t, arg));
}
