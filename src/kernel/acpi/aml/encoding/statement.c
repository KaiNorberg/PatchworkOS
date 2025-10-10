#include "statement.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_token.h"
#include "expression.h"
#include "package_length.h"
#include "term.h"
#include "log/log.h"

#include <errno.h>

uint64_t aml_predicate_read(aml_term_list_ctx_t* ctx, aml_integer_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_else_read(aml_term_list_ctx_t* ctx, bool shouldExecute)
{
    if (aml_token_expect(ctx, AML_ELSE_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ElseOp");
        return ERR;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    if (shouldExecute)
    {
        // Execute the TermList in the same scope
        if (aml_term_list_read(ctx->state, ctx->scope, ctx->current, end, ctx) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read TermList");
            return ERR;
        }
    }

    ctx->current = end;

    return 0;
}

uint64_t aml_def_if_else_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_IF_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read IfOp");
        return ERR;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return ERR;
    }

    // The end of the If statement, the "Else" part is not included in this length, see section 5.4.1 figure 5.17 of
    // the ACPI spec.
    const uint8_t* end = start + pkgLength;

    aml_integer_t predicate;
    if (aml_predicate_read(ctx, &predicate) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Predicate");
        return ERR;
    }

    bool isTrue = predicate != AML_FALSE;
    if (isTrue)
    {
        // Execute the TermList in the same scope
        if (aml_term_list_read(ctx->state, ctx->scope, ctx->current, end, ctx) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read TermList");
            return ERR;
        }
    }

    ctx->current = end;

    aml_token_t elseOp;
    aml_token_peek(ctx, &elseOp);

    if (elseOp.num == AML_ELSE_OP) // Optional
    {
        if (aml_def_else_read(ctx, !isTrue) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ElseOp");
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_def_noop_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_NOOP_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NoopOp");
        return ERR;
    }

    return 0;
}

aml_object_t* aml_arg_object_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* result = aml_term_arg_read(ctx, AML_DATA_REF_OBJECTS);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    return result; // Transfer ownership
}

uint64_t aml_def_return_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_RETURN_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ReturnOp");
        return ERR;
    }

    aml_object_t* argObject = aml_arg_object_read(ctx);
    if (argObject == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ArgObject");
        return ERR;
    }
    DEREF_DEFER(argObject);

    ctx->stopReason = AML_STOP_REASON_RETURN;
    aml_state_result_set(ctx->state, argObject);

    return 0;
}

uint64_t aml_def_release_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_RELEASE_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ReleaseOp");
        return ERR;
    }

    aml_object_t* mutexObject = aml_mutex_object_read(ctx);
    if (mutexObject == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MutexObject");
        return ERR;
    }
    DEREF_DEFER(mutexObject);

    assert(mutexObject->type == AML_MUTEX);

    if (aml_mutex_release(&mutexObject->mutex.mutex) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to release mutex");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_break_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_BREAK_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BreakOp");
        return ERR;
    }

    ctx->stopReason = AML_STOP_REASON_BREAK;
    return 0;
}

uint64_t aml_def_continue_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_CONTINUE_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ContinueOp");
        return ERR;
    }

    ctx->stopReason = AML_STOP_REASON_CONTINUE;
    return 0;
}

uint64_t aml_def_while_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_WHILE_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read WhileOp");
        return ERR;
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(ctx, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    while (true)
    {
        const uint8_t* loopStart = ctx->current;

        aml_integer_t predicate;
        if (aml_predicate_read(ctx, &predicate) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read Predicate");
            return ERR;
        }

        if (predicate == AML_FALSE)
        {
            break;
        }

        // Execute the TermList in the same scope, might change flow control
        if (aml_term_list_read(ctx->state, ctx->scope, ctx->current, end, ctx) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read TermList");
            return ERR;
        }

        ctx->current = loopStart;

        if (ctx->stopReason == AML_STOP_REASON_RETURN || ctx->stopReason == AML_STOP_REASON_NONE)
        {
            break;
        }
        else if (ctx->stopReason == AML_STOP_REASON_BREAK)
        {
            ctx->stopReason = AML_STOP_REASON_NONE;
            break;
        }
        else if (ctx->stopReason == AML_STOP_REASON_CONTINUE)
        {
            ctx->stopReason = AML_STOP_REASON_NONE;
        }
        else
        {
            AML_DEBUG_ERROR(ctx, "Invalid flow control state in while loop");
            errno = EILSEQ;
            return ERR;
        }
    }

    ctx->current = end;
    return 0;
}

uint64_t aml_statement_opcode_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    uint64_t result = 0;
    switch (op.num)
    {
    case AML_IF_OP:
        result = aml_def_if_else_read(ctx);
        break;
    case AML_NOOP_OP:
        result = aml_def_noop_read(ctx);
        break;
    case AML_RETURN_OP:
        result = aml_def_return_read(ctx);
        break;
    case AML_RELEASE_OP:
        result = aml_def_release_read(ctx);
        break;
    case AML_WHILE_OP:
        result = aml_def_while_read(ctx);
        break;
    case AML_BREAK_OP:
        result = aml_def_break_read(ctx);
        break;
    case AML_CONTINUE_OP:
        result = aml_def_continue_read(ctx);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Unknown StatementOpcode '%s' (0x%x)", op.props->name, op.num);
        errno = ENOSYS;
        return ERR;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read StatementOpcode '%s' (0x%x)", op.props->name, op.num);
        return ERR;
    }

    return 0;
}
