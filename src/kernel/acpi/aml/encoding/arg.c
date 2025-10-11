#include "arg.h"

#include "acpi/aml/debug.h"
#include "acpi/aml/state.h"
#include "acpi/aml/token.h"
#include "expression.h"

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
    return REF(CONTAINER_OF(ctx->state->args[index], aml_object_t, arg));
}
