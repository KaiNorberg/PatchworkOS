#include <kernel/acpi/aml/encoding/term.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/data.h>
#include <kernel/acpi/aml/encoding/expression.h>
#include <kernel/acpi/aml/encoding/named.h>
#include <kernel/acpi/aml/encoding/namespace_modifier.h>
#include <kernel/acpi/aml/encoding/statement.h>
#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>

#include <kernel/utils/ref.h>
#include <stdint.h>

status_t aml_term_arg_read(aml_term_list_ctx_t* ctx, aml_type_t allowedTypes, aml_object_t** out)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    status_t status = OK;
    aml_object_t* object = NULL;
    switch (op.props->type)
    {
    case AML_TOKEN_TYPE_EXPRESSION:
    case AML_TOKEN_TYPE_NAME: // MethodInvocation is a Name
        status = aml_expression_opcode_read(ctx, &object);
        break;
    case AML_TOKEN_TYPE_ARG:
        status = aml_arg_obj_read(ctx, &object);
        break;
    case AML_TOKEN_TYPE_LOCAL:
        status = aml_local_obj_read(ctx, &object);
        break;
    default:
        object = aml_object_new();
        if (object == NULL)
        {
            status = ERR(ACPI, NOMEM);
            break;
        }

        status = aml_data_object_read(ctx, object);
        if (IS_ERR(status))
        {
            UNREF(object);
            object = NULL;
            break;
        }
    }

    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", op.props->name);
        return status;
    }
    UNREF_DEFER(object);

    return aml_convert_source(ctx->state, object, out, allowedTypes);
}

status_t aml_term_arg_read_integer(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    aml_object_t* temp = NULL;
    status_t status = aml_term_arg_read(ctx, AML_INTEGER, &temp);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    assert(temp->type == AML_INTEGER);

    *out = temp->integer.value;
    UNREF(temp);
    return OK;
}

status_t aml_term_arg_read_string(aml_term_list_ctx_t* ctx, aml_string_t** out)
{
    aml_object_t* temp = NULL;
    status_t status = aml_term_arg_read(ctx, AML_STRING, &temp);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    assert(temp->type == AML_STRING);

    *out = &temp->string; // Transfer ownership
    return OK;
}

status_t aml_term_arg_read_buffer(aml_term_list_ctx_t* ctx, aml_buffer_t** out)
{
    aml_object_t* temp = NULL;
    status_t status = aml_term_arg_read(ctx, AML_BUFFER, &temp);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    assert(temp->type == AML_BUFFER);

    *out = &temp->buffer; // Transfer ownership
    return OK;
}

status_t aml_term_arg_read_package(aml_term_list_ctx_t* ctx, aml_package_t** out)
{
    aml_object_t* temp = NULL;
    status_t status = aml_term_arg_read(ctx, AML_PACKAGE, &temp);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    assert(temp->type == AML_PACKAGE);

    *out = &temp->package; // Transfer ownership
    return OK;
}

status_t aml_object_read(aml_term_list_ctx_t* ctx)
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
        return ERR(ACPI, ILSEQ);
    }
}

status_t aml_term_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t token;
    aml_token_peek(ctx, &token);

    status_t status = OK;
    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_STATEMENT:
        status = aml_statement_opcode_read(ctx);
        break;
    case AML_TOKEN_TYPE_NAME: // MethodInvocation is a Name
    case AML_TOKEN_TYPE_EXPRESSION:
    {
        aml_object_t* expression = NULL;
        status = aml_expression_opcode_read(ctx, &expression);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ExpressionOpcode");
            break;
        }
        // Set the result of the state to the last evaluated expression, check `aml_method_invoke()` for more details.
        // We cant just do this in `aml_expression_opcode_read()` because predicates are not supposed to be considered
        // for implicit return.
        // aml_state_result_set(ctx->state, result);
        UNREF(expression);
        break;
    }
    default:
        status = aml_object_read(ctx);
        break;
    }

    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermObj '%s' (0x%x)", token.props->name, token.num);
        return status;
    }

    return OK;
}

status_t aml_term_list_read(aml_state_t* state, aml_object_t* scope, const uint8_t* start, const uint8_t* end,
    aml_term_list_ctx_t* parentCtx)
{
    if (state == NULL || scope == NULL || start == NULL || end == NULL || start > end)
    {
        return ERR(ACPI, INVAL);
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
        status_t status = aml_term_obj_read(&ctx);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    if (parentCtx != NULL)
    {
        parentCtx->stopReason = ctx.stopReason;
    }
    return OK;
}
