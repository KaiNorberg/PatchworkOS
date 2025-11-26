#include <kernel/acpi/aml/encoding/term.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/data.h>
#include <kernel/acpi/aml/encoding/expression.h>
#include <kernel/acpi/aml/encoding/named.h>
#include <kernel/acpi/aml/encoding/namespace_modifier.h>
#include <kernel/acpi/aml/encoding/statement.h>
#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/tests.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>

#include <errno.h>
#include <stdint.h>

aml_object_t* aml_term_arg_read(aml_term_list_ctx_t* ctx, aml_type_t allowedTypes)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    aml_object_t* value = NULL;
    switch (op.props->type)
    {
    case AML_TOKEN_TYPE_EXPRESSION:
    case AML_TOKEN_TYPE_NAME: // MethodInvocation is a Name
        value = aml_expression_opcode_read(ctx);
        break;
    case AML_TOKEN_TYPE_ARG:
        value = aml_arg_obj_read(ctx);
        break;
    case AML_TOKEN_TYPE_LOCAL:
        value = aml_local_obj_read(ctx);
        break;
    default:
        value = aml_object_new();
        if (value == NULL)
        {
            break;
        }

        if (aml_data_object_read(ctx, value) == ERR)
        {
            DEREF(value);
            value = NULL;
            break;
        }
    }

    if (value == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", op.props->name);
        return NULL;
    }
    DEREF_DEFER(value);

    aml_object_t* out = NULL;
    if (aml_convert_source(ctx->state, value, &out, allowedTypes) == ERR)
    {
        return NULL;
    }

    assert(out->type & allowedTypes);

    return out; // Transfer ownership
}

uint64_t aml_term_arg_read_integer(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    aml_object_t* temp = aml_term_arg_read(ctx, AML_INTEGER);
    if (temp == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    assert(temp->type == AML_INTEGER);

    *out = temp->integer.value;
    DEREF(temp);
    return 0;
}

aml_string_t* aml_term_arg_read_string(aml_term_list_ctx_t* ctx)
{
    aml_object_t* temp = aml_term_arg_read(ctx, AML_STRING);
    if (temp == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    assert(temp->type == AML_STRING);

    return &temp->string; // Transfer ownership
}

aml_buffer_t* aml_term_arg_read_buffer(aml_term_list_ctx_t* ctx)
{
    aml_object_t* temp = aml_term_arg_read(ctx, AML_BUFFER);
    if (temp == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    assert(temp->type == AML_BUFFER);

    return &temp->buffer; // Transfer ownership
}

aml_package_t* aml_term_arg_read_package(aml_term_list_ctx_t* ctx)
{
    aml_object_t* temp = aml_term_arg_read(ctx, AML_PACKAGE);
    if (temp == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    assert(temp->type == AML_PACKAGE);

    return &temp->package; // Transfer ownership
}

uint64_t aml_object_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_NAMESPACE_MODIFIER:
        return aml_namespace_modifier_obj_read(ctx);
    case AML_TOKEN_TYPE_NAMED:
        return aml_named_obj_read(ctx);
    default:
        AML_DEBUG_ERROR(ctx, "Invalid token type '%s'", aml_token_type_to_string(token.props->type));
        errno = EILSEQ;
        return ERR;
    }
}

uint64_t aml_term_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    uint64_t result = 0;
    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_STATEMENT:
        result = aml_statement_opcode_read(ctx);
        break;
    case AML_TOKEN_TYPE_NAME: // MethodInvocation is a Name
    case AML_TOKEN_TYPE_EXPRESSION:
    {
        aml_object_t* expression = aml_expression_opcode_read(ctx);
        if (expression == NULL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ExpressionOpcode");
            result = ERR;
            break;
        }
        // Set the result of the state to the last evaluated expression, check `aml_method_invoke()` for more details.
        // We cant just do this in `aml_expression_opcode_read()` because predicates are not supposed to be considered
        // for implicit return.
        // aml_state_result_set(ctx->state, result);
        DEREF(expression);
        result = 0;
        break;
    }
    default:
        result = aml_object_read(ctx);
        break;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermObj '%s' (0x%x)", token.props->name, token.num);
        return ERR;
    }

    return 0;
}

uint64_t aml_term_list_read(aml_state_t* state, aml_object_t* scope, const uint8_t* start, const uint8_t* end,
    aml_term_list_ctx_t* parentCtx)
{
    if (state == NULL || scope == NULL || start == NULL || end == NULL || start > end)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_term_list_ctx_t ctx = {
        .state = state,
        .scope = scope,
        .start = start,
        .end = end,
        .current = start,
        .stopReason = AML_STOP_REASON_NONE,
    };

    while (ctx.end > ctx.current && ctx.stopReason == AML_STOP_REASON_NONE)
    {
        // End of buffer not reached => byte is not nothing => must be a termobj.
        if (aml_term_obj_read(&ctx) == ERR)
        {
            return ERR;
        }
    }

    if (parentCtx != NULL)
    {
        parentCtx->stopReason = ctx.stopReason;
    }
    return 0;
}
