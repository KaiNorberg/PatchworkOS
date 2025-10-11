#include "term.h"

#include "acpi/aml/debug.h"
#include "acpi/aml/exception.h"
#include "acpi/aml/state.h"
#include "acpi/aml/token.h"

#include "acpi/aml/runtime/convert.h"
#include "data.h"
#include "expression.h"
#include "named.h"
#include "namespace_modifier.h"
#include "statement.h"

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
        value = aml_object_new(ctx);
        if (value == NULL)
        {
            return NULL;
        }

        if (aml_data_object_read(ctx, value) == ERR)
        {
            DEREF(value);
            AML_DEBUG_ERROR(ctx, "Failed to read DataObject");
            return NULL;
        }
    }

    if (value == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", op.props->name);
        return NULL;
    }
    DEREF_DEFER(value);

    if (value->flags & AML_OBJECT_EXCEPTION_ON_USE)
    {
        AML_EXCEPTION_RAISE(AML_ERROR); // Not fatal.
        value->flags &= ~AML_OBJECT_EXCEPTION_ON_USE;
        // We can still use the object, so continue.
    }

    aml_object_t* out = NULL;
    if (aml_convert_source(value, &out, allowedTypes) == ERR)
    {
        return NULL;
    }

    assert(out->type & allowedTypes);

    return out; // Transfer ownership
}

uint64_t aml_term_arg_read_integer(aml_term_list_ctx_t* ctx, aml_integer_t* out)
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

aml_string_obj_t* aml_term_arg_read_string(aml_term_list_ctx_t* ctx)
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

    switch (token.props->type)
    {
    case AML_TOKEN_TYPE_STATEMENT:
        if (aml_statement_opcode_read(ctx) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read StatementOpcode");
            return ERR;
        }
        return 0;
    case AML_TOKEN_TYPE_NAME: // MethodInvocation is a Name
    case AML_TOKEN_TYPE_EXPRESSION:
    {
        aml_object_t* result = aml_expression_opcode_read(ctx);
        if (result == NULL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ExpressionOpcode");
            return ERR;
        }
        DEREF(result);
        return 0;
    }
    default:
        if (aml_object_read(ctx) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read Object");
            return ERR;
        }
        return 0;
    }
}

uint64_t aml_term_list_read(aml_state_t* state, aml_object_t* scope, const uint8_t* start, const uint8_t* end,
    aml_term_list_ctx_t* parentCtx)
{
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
