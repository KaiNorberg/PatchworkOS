#include <kernel/acpi/aml/encoding/statement.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/expression.h>
#include <kernel/acpi/aml/encoding/package_length.h>
#include <kernel/acpi/aml/encoding/term.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/log/log.h>

status_t aml_predicate_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }
    return OK;
}

status_t aml_def_else_read(aml_term_list_ctx_t* ctx, bool shouldExecute)
{
    if (!aml_token_expect(ctx, AML_ELSE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ElseOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    if (shouldExecute)
    {
        // Execute the TermList in the same scope
        status = aml_term_list_read(ctx->state, ctx->scope, ctx->current, end, ctx);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read TermList");
            return status;
        }
    }

    ctx->current = end;

    return OK;
}

status_t aml_def_if_else_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_IF_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read IfOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    // The end of the If statement, the "Else" part is not included in this length, see section 5.4.1 figure 5.17 of
    // the ACPI spec.
    const uint8_t* end = start + pkgLength;

    aml_uint_t predicate;
    status = aml_predicate_read(ctx, &predicate);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Predicate");
        return status;
    }

    bool isTrue = predicate != AML_FALSE;
    if (isTrue)
    {
        // Execute the TermList in the same scope
        status = aml_term_list_read(ctx->state, ctx->scope, ctx->current, end, ctx);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read TermList");
            return status;
        }
    }

    if (ctx->stopReason != AML_STOP_REASON_NONE)
    {
        return OK;
    }

    ctx->current = end;

    aml_token_t elseOp;
    aml_token_peek(ctx, &elseOp);

    if (elseOp.num == AML_ELSE_OP) // Optional
    {
        status = aml_def_else_read(ctx, !isTrue);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read ElseOp");
            return status;
        }
    }

    return OK;
}

status_t aml_def_noop_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_NOOP_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read NoopOp");
        return ERR(ACPI, ILSEQ);
    }

    return OK;
}

status_t aml_arg_object_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status = aml_term_arg_read(ctx, AML_DATA_REF_OBJECTS, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK; // Transfer ownership
}

status_t aml_def_return_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_RETURN_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ReturnOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* argObject = NULL;
    status_t status = aml_arg_object_read(ctx, &argObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ArgObject");
        return status;
    }
    UNREF_DEFER(argObject);

    ctx->stopReason = AML_STOP_REASON_RETURN;
    aml_state_result_set(ctx->state, argObject);

    return OK;
}

status_t aml_def_release_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_RELEASE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ReleaseOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* mutexObject = NULL;
    status_t status = aml_mutex_object_read(ctx, &mutexObject);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MutexObject");
        return status;
    }
    UNREF_DEFER(mutexObject);

    assert(mutexObject->type == AML_MUTEX);

    status = aml_mutex_release(&mutexObject->mutex.mutex);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to release mutex");
        return status;
    }

    return OK;
}

status_t aml_def_break_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_BREAK_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BreakOp");
        return ERR(ACPI, ILSEQ);
    }

    ctx->stopReason = AML_STOP_REASON_BREAK;
    return OK;
}

status_t aml_def_continue_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_CONTINUE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ContinueOp");
        return ERR(ACPI, ILSEQ);
    }

    ctx->stopReason = AML_STOP_REASON_CONTINUE;
    return OK;
}

status_t aml_def_while_read(aml_term_list_ctx_t* ctx)
{
    if (!aml_token_expect(ctx, AML_WHILE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read WhileOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status_t status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    const uint8_t* loopStart = ctx->current;
    while (true)
    {
        ctx->current = loopStart;

        aml_uint_t predicate;
        status = aml_predicate_read(ctx, &predicate);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read Predicate");
            return status;
        }

        if (predicate == AML_FALSE)
        {
            break;
        }

        // Execute the TermList in the same scope, might change flow control
        status = aml_term_list_read(ctx->state, ctx->scope, ctx->current, end, ctx);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read TermList");
            return status;
        }

        if (ctx->stopReason == AML_STOP_REASON_NONE)
        {
            continue;
        }
        if (ctx->stopReason == AML_STOP_REASON_RETURN)
        {
            break;
        }
        if (ctx->stopReason == AML_STOP_REASON_BREAK)
        {
            ctx->stopReason = AML_STOP_REASON_NONE;
            break;
        }
        if (ctx->stopReason == AML_STOP_REASON_CONTINUE)
        {
            ctx->stopReason = AML_STOP_REASON_NONE;
            continue;
        }

        AML_DEBUG_ERROR(ctx, "Invalid flow control state in while loop");
        return ERR(ACPI, ILSEQ);
    }

    ctx->current = end;
    return OK;
}

status_t aml_statement_opcode_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    status_t status = OK;
    switch (op.num)
    {
    case AML_IF_OP:
        status = aml_def_if_else_read(ctx);
        break;
    case AML_NOOP_OP:
        status = aml_def_noop_read(ctx);
        break;
    case AML_RETURN_OP:
        status = aml_def_return_read(ctx);
        break;
    case AML_RELEASE_OP:
        status = aml_def_release_read(ctx);
        break;
    case AML_WHILE_OP:
        status = aml_def_while_read(ctx);
        break;
    case AML_BREAK_OP:
        status = aml_def_break_read(ctx);
        break;
    case AML_CONTINUE_OP:
        status = aml_def_continue_read(ctx);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Unknown StatementOpcode '%s' (0x%x)", op.props->name, op.num);
        return ERR(ACPI, IMPL);
    }

    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read StatementOpcode '%s' (0x%x)", op.props->name, op.num);
        return status;
    }

    return OK;
}
