#include <kernel/acpi/aml/encoding/expression.h>

#include <kernel/acpi/aml/debug.h>
#include <kernel/acpi/aml/encoding/arg.h>
#include <kernel/acpi/aml/encoding/debug.h>
#include <kernel/acpi/aml/encoding/package_length.h>
#include <kernel/acpi/aml/encoding/term.h>
#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/aml/runtime/compare.h>
#include <kernel/acpi/aml/runtime/concat.h>
#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/acpi/aml/runtime/copy.h>
#include <kernel/acpi/aml/runtime/method.h>
#include <kernel/acpi/aml/runtime/mid.h>
#include <kernel/acpi/aml/runtime/store.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/log/log.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/timer.h>

#include <sys/proc.h>

status_t aml_operand_read(aml_term_list_ctx_t* ctx, aml_type_t allowedTypes, aml_object_t** out)
{
    status_t status = aml_term_arg_read(ctx, allowedTypes, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK; // Transfer ownership
}

static inline status_t aml_op_operand_operand_target_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand1, aml_object_t** operand2, aml_object_t** target)
{
    status_t status;

    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status = aml_operand_read(ctx, allowedTypes, operand1);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand1");
        return status;
    }

    // Operand2 must be the same type as operand1.
    status = aml_operand_read(ctx, (*operand1)->type, operand2);
    if (IS_ERR(status))
    {
        UNREF(*operand1);
        AML_DEBUG_ERROR(ctx, "Failed to read operand2");
        return status;
    }

    status = aml_target_read_and_resolve(ctx, target);
    if (IS_ERR(status))
    {
        UNREF(*operand1);
        UNREF(*operand2);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }

    return OK;
}

static inline status_t aml_op_operand_operand_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand1, aml_object_t** operand2)
{
    status_t status;

    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status = aml_operand_read(ctx, allowedTypes, operand1);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand1");
        return status;
    }

    // Operand2 must be the same type as operand1.
    status = aml_operand_read(ctx, (*operand1)->type, operand2);
    if (IS_ERR(status))
    {
        UNREF(*operand1);
        AML_DEBUG_ERROR(ctx, "Failed to read operand2");
        return status;
    }

    return OK;
}

static inline status_t aml_op_operand_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand)
{
    status_t status;

    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status = aml_operand_read(ctx, allowedTypes, operand);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand");
        return status;
    }

    return OK;
}

static inline status_t aml_op_operand_target_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand, aml_object_t** target)
{
    status_t status;

    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status = aml_operand_read(ctx, allowedTypes, operand);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand");
        return status;
    }

    status = aml_target_read_and_resolve(ctx, target);
    if (IS_ERR(status))
    {
        UNREF(*operand);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }

    return OK;
}

static inline status_t aml_op_operand_shiftcount_target_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand, aml_uint_t* shiftCount, aml_object_t** target)
{
    status_t status;

    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status = aml_operand_read(ctx, allowedTypes, operand);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand");
        return status;
    }

    status = aml_shift_count_read(ctx, shiftCount);
    if (IS_ERR(status))
    {
        UNREF(*operand);
        AML_DEBUG_ERROR(ctx, "Failed to read ShiftCount");
        return status;
    }

    status = aml_target_read_and_resolve(ctx, target);
    if (IS_ERR(status))
    {
        UNREF(*operand);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }

    return OK;
}

static inline status_t aml_op_data_data_target_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_object_t** data1, aml_object_t** data2, aml_object_t** target)
{
    status_t status;

    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status = aml_data_read(ctx, data1);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read data1");
        return status;
    }

    status = aml_data_read(ctx, data2);
    if (IS_ERR(status))
    {
        UNREF(*data1);
        AML_DEBUG_ERROR(ctx, "Failed to read data2");
        return status;
    }

    status = aml_target_read_and_resolve(ctx, target);
    if (IS_ERR(status))
    {
        UNREF(*data1);
        UNREF(*data2);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }

    return OK;
}

static inline status_t aml_op_termarg_simplename_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** termarg, aml_object_t** simplename)
{
    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status_t status = aml_term_arg_read(ctx, allowedTypes, termarg);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    status = aml_simple_name_read_and_resolve(ctx, simplename);
    if (IS_ERR(status))
    {
        UNREF(*termarg);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SimpleName");
        return status;
    }

    return OK;
}

static inline status_t aml_op_supername_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_object_t** supername)
{
    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status_t status = aml_super_name_read_and_resolve(ctx, supername);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
        return status;
    }

    return OK;
}

static inline status_t aml_op_termarg_supername_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** termarg, aml_object_t** supername)
{
    if (!aml_token_expect(ctx, expectedOp))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR(ACPI, ILSEQ);
    }

    status_t status = aml_term_arg_read(ctx, allowedTypes, termarg);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    status = aml_super_name_read_and_resolve(ctx, supername);
    if (IS_ERR(status))
    {
        UNREF(*termarg);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
        return status;
    }

    return OK;
}

status_t aml_buffer_size_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }
    return OK;
}

status_t aml_def_buffer_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    status_t status;

    if (!aml_token_expect(ctx, AML_BUFFER_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BufferOp");
        return ERR(ACPI, ILSEQ);
    }

    const uint8_t* start = ctx->current;

    aml_pkg_length_t pkgLength;
    status = aml_pkg_length_read(ctx, &pkgLength);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read PkgLength");
        return status;
    }

    const uint8_t* end = start + pkgLength;

    aml_uint_t bufferSize;
    status = aml_buffer_size_read(ctx, &bufferSize);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BufferSize");
        return status;
    }

    uint64_t availableBytes = (uint64_t)(end - ctx->current);

    status = aml_buffer_set(out, ctx->current, availableBytes, bufferSize);
    if (IS_ERR(status))
    {
        return status;
    }

    ctx->current = end;
    return OK;
}

status_t aml_term_arg_list_read(aml_term_list_ctx_t* ctx, uint64_t argCount, aml_term_arg_list_t* out)
{
    if (argCount > AML_MAX_ARGS)
    {
        return ERR(ACPI, ILSEQ);
    }

    uint64_t i = 0;
    for (; i < argCount; i++)
    {
        status_t status = aml_term_arg_read(ctx, AML_DATA_REF_OBJECTS, &out->args[i]);
        if (IS_ERR(status))
        {
            for (uint64_t j = 0; j < i; j++)
            {
                UNREF(out->args[j]);
                out->args[j] = NULL;
            }
            return status;
        }
    }
    out->args[i] = NULL;

    return OK;
}

status_t aml_method_invocation_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* target;
    status_t status = aml_name_string_read_and_resolve(ctx, &target);
    if (target == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve NameString");
        return status;
    }
    UNREF_DEFER(target);

    if (target->type == AML_METHOD)
    {
        aml_term_arg_list_t args = {0};
        status = aml_term_arg_list_read(ctx, target->method.methodFlags.argCount, &args);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to read method arguments");
            return status;
        }

        aml_object_t* result = NULL;
        status_t status = aml_method_invoke(ctx->state, &target->method, args.args, &result);
        if (IS_ERR(status))
        {
            for (uint8_t i = 0; args.args[i] != NULL; i++)
            {
                UNREF(args.args[i]);
                args.args[i] = NULL;
            }
            AML_DEBUG_ERROR(ctx, "Failed to evaluate method '%s'", AML_NAME_TO_STRING(target->name));
            return status;
        }

        for (uint8_t i = 0; args.args[i] != NULL; i++)
        {
            UNREF(args.args[i]);
            args.args[i] = NULL;
        }

        aml_state_result_set(ctx->state, result);

        *out = result; // Transfer ownership
        return OK;
    }

    // Note that just resolving an object does not set the implicit return value.
    *out = REF(target);
    return OK;
}

status_t aml_def_cond_ref_of_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    if (!aml_token_expect(ctx, AML_COND_REF_OF_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read CondRefOfOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* source = NULL;
    status_t status = aml_super_name_read_and_resolve(ctx, &source);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
        return status;
    }
    UNREF_DEFER(source);

    aml_object_t* result = NULL;
    status = aml_target_read_and_resolve(ctx, &result);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }
    UNREF_DEFER(result);

    aml_object_t* output = aml_object_new();
    if (output == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(output);

    if (source == NULL)
    {
        // Return false since the source did not resolve to an object.
        status = aml_integer_set(output, 0);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to init false integer");
            return status;
        }
        *out = REF(output);
        return OK;
    }

    if (result == NULL)
    {
        // Return true since source resolved to an object and result dident so we dont need to store anything.
        status = aml_integer_set(output, AML_TRUE);
        if (IS_ERR(status))
        {
            AML_DEBUG_ERROR(ctx, "Failed to init true integer");
            return status;
        }
        *out = REF(output);
        return OK;
    }

    // Store a reference to source in the result and return true.

    status = aml_object_reference_set(result, source);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to init ObjectReference in result");
        return status;
    }

    status = aml_integer_set(output, AML_TRUE);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to init true integer");
        return status;
    }

    *out = REF(output);
    return OK;
}

status_t aml_def_store_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* source = NULL;
    aml_object_t* destination = NULL;
    status_t status = aml_op_termarg_supername_read(ctx, AML_STORE_OP, AML_DATA_REF_OBJECTS, &source, &destination);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefStore structure");
        return status;
    }
    UNREF_DEFER(source);
    UNREF_DEFER(destination);

    status = aml_store(ctx->state, source, destination);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to store source '%s' in destination '%s'", AML_NAME_TO_STRING(source->name),
            AML_NAME_TO_STRING(destination->name));
        return status;
    }

    *out = REF(source);
    return OK;
}

status_t aml_dividend_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_divisor_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_remainder_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* result = NULL;
    status_t status = aml_target_read_and_resolve(ctx, &result);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }

    *out = result; // Transfer ownership
    return OK;
}

status_t aml_quotient_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* result = NULL;
    status_t status = aml_target_read_and_resolve(ctx, &result);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }

    *out = result; // Transfer ownership
    return OK;
}

status_t aml_def_add_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    status = aml_op_operand_operand_target_read(ctx, AML_ADD_OP, AML_INTEGER, &operand1, &operand2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefAdd structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, operand1->integer.value + operand2->integer.value);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to set integer value");
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to store result");
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_subtract_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    status = aml_op_operand_operand_target_read(ctx, AML_SUBTRACT_OP, AML_INTEGER, &operand1, &operand2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefSubtract structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, operand1->integer.value - operand2->integer.value);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to set integer value");
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to store result");
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_multiply_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    status = aml_op_operand_operand_target_read(ctx, AML_MULTIPLY_OP, AML_INTEGER, &operand1, &operand2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefMultiply structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, operand1->integer.value * operand2->integer.value);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to set integer value");
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to store result");
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_divide_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    if (!aml_token_expect(ctx, AML_DIVIDE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DivideOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_uint_t dividend;
    status = aml_dividend_read(ctx, &dividend);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Dividend");
        return status;
    }

    aml_uint_t divisor;
    status = aml_divisor_read(ctx, &divisor);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Divisor");
        return status;
    }

    if (divisor == 0)
    {
        divisor = 1;
    }

    aml_object_t* remainderDest = NULL;
    status = aml_remainder_read(ctx, &remainderDest);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read remainder");
        return status;
    }
    UNREF_DEFER(remainderDest);

    aml_object_t* quotientDest = NULL;
    status = aml_quotient_read(ctx, &quotientDest);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read quotient");
        return status;
    }
    UNREF_DEFER(quotientDest);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    // Init with remainder.
    status = aml_integer_set(result, dividend % divisor);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to init remainder");
        return status;
    }

    status = aml_store(ctx->state, result, remainderDest);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to store remainder");
        return status;
    }

    // Init with quotient.
    status = aml_integer_set(result, dividend / divisor);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to init quotient");
        return status;
    }

    status = aml_store(ctx->state, result, quotientDest);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to store quotient");
        return status;
    }

    // Quotient stays in result.
    *out = REF(result);
    return OK;
}

status_t aml_def_mod_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    if (!aml_token_expect(ctx, AML_MOD_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ModOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_uint_t dividend;
    status = aml_dividend_read(ctx, &dividend);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Dividend");
        return status;
    }

    aml_uint_t divisor;
    status = aml_divisor_read(ctx, &divisor);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Divisor");
        return status;
    }

    aml_object_t* target = NULL;
    status = aml_target_read_and_resolve(ctx, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }
    UNREF_DEFER(target);

    if (divisor == 0)
    {
        divisor = 1;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, dividend % divisor);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_and_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    status = aml_op_operand_operand_target_read(ctx, AML_AND_OP, AML_INTEGER, &operand1, &operand2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefAnd structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, operand1->integer.value & operand2->integer.value);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to set integer value");
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_nand_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    status = aml_op_operand_operand_target_read(ctx, AML_NAND_OP, AML_INTEGER, &operand1, &operand2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefNand structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, ~(operand1->integer.value & operand2->integer.value));
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to set integer value");
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_or_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    status = aml_op_operand_operand_target_read(ctx, AML_OR_OP, AML_INTEGER, &operand1, &operand2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefOr structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, operand1->integer.value | operand2->integer.value);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_nor_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    status = aml_op_operand_operand_target_read(ctx, AML_NOR_OP, AML_INTEGER, &operand1, &operand2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefNor structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, ~(operand1->integer.value | operand2->integer.value));
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_xor_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    status = aml_op_operand_operand_target_read(ctx, AML_XOR_OP, AML_INTEGER, &operand1, &operand2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefXor structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, operand1->integer.value ^ operand2->integer.value);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_not_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    status_t status = aml_op_operand_target_read(ctx, AML_NOT_OP, AML_INTEGER, &operand, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefNot structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, ~operand->integer.value);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_shift_count_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_def_shift_left_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    aml_uint_t shiftCount;
    status = aml_op_operand_shiftcount_target_read(ctx, AML_SHIFT_LEFT_OP, AML_INTEGER, &operand, &shiftCount, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefShiftLeft structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    // C will discard the most significant bits
    if (shiftCount >= aml_integer_bit_size())
    {
        status = aml_integer_set(result, 0);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    else
    {
        aml_uint_t operandValue = operand->integer.value;
        status = aml_integer_set(result, operandValue << shiftCount);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_shift_right_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    aml_uint_t shiftCount;
    status =
        aml_op_operand_shiftcount_target_read(ctx, AML_SHIFT_RIGHT_OP, AML_INTEGER, &operand, &shiftCount, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefShiftRight structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    // C will zero the most significant bits
    if (shiftCount >= aml_integer_bit_size())
    {
        status = aml_integer_set(result, 0);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    else
    {
        aml_uint_t operandValue = operand->integer.value;
        status = aml_integer_set(result, operandValue >> shiftCount);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_increment_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* superName = NULL;
    status = aml_op_supername_read(ctx, AML_INCREMENT_OP, &superName);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefIncrement structure");
        return status;
    }
    UNREF_DEFER(superName);

    aml_object_t* source = NULL;
    status = aml_convert_source(ctx->state, superName, &source, AML_INTEGER);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(source);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, source->integer.value + 1);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_convert_result(ctx->state, result, superName);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_decrement_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* superName = NULL;
    status = aml_op_supername_read(ctx, AML_DECREMENT_OP, &superName);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefDecrement structure");
        return status;
    }
    UNREF_DEFER(superName);

    aml_object_t* source = NULL;
    status = aml_convert_source(ctx->state, superName, &source, AML_INTEGER);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(source);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, source->integer.value - 1);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_convert_result(ctx->state, result, superName);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_obj_reference_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* termArg = NULL;
    status_t status = aml_term_arg_read(ctx, AML_OBJECT_REFERENCE | AML_STRING, &termArg);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }
    UNREF_DEFER(termArg);

    if (termArg->type == AML_OBJECT_REFERENCE)
    {
        *out = REF(termArg->objectReference.target);
        return OK;
    }

    if (termArg->type == AML_STRING)
    {
        aml_object_t* target = aml_namespace_find_by_path(&ctx->state->overlay, ctx->scope, termArg->string.content);
        if (target == NULL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to find target scope '%s'", termArg->string.content);
            return ERR(ACPI, ILSEQ);
        }

        *out = target; // Transfer ownership
        return OK;
    }

    // Should never happen.
    return ERR(ACPI, ILSEQ);
}

status_t aml_def_deref_of_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    if (!aml_token_expect(ctx, AML_DEREF_OF_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DerefOfOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* obj = NULL;
    status = aml_obj_reference_read(ctx, &obj);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ObjReference");
        return status;
    }

    *out = obj; // Transfer ownership
    return OK;
}

status_t aml_buff_pkg_str_obj_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status = aml_term_arg_read(ctx, AML_BUFFER | AML_PACKAGE | AML_STRING, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK; // Transfer ownership
}

status_t aml_index_value_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_def_index_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    if (!aml_token_expect(ctx, AML_INDEX_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read IndexOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* buffPkgStrObj = NULL;
    status = aml_buff_pkg_str_obj_read(ctx, &buffPkgStrObj);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BuffPkgStrObj");
        return status;
    }
    UNREF_DEFER(buffPkgStrObj);

    aml_uint_t index;
    status = aml_index_value_read(ctx, &index);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read IndexValue");
        return status;
    }

    aml_object_t* target = NULL;
    status = aml_target_read_and_resolve(ctx, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    switch (buffPkgStrObj->type)
    {
    case AML_PACKAGE: // Section 19.6.63.1
    {
        aml_package_t* package = &buffPkgStrObj->package;

        if (index >= package->length)
        {
            AML_DEBUG_ERROR(ctx, "Index out of bounds for package (length %llu, index %llu)", package->length, index);
            return ERR(ACPI, ILSEQ);
        }

        status = aml_object_reference_set(result, package->elements[index]);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    break;
    case AML_BUFFER: // Section 19.6.63.2
    {
        if (index >= buffPkgStrObj->buffer.length)
        {
            AML_DEBUG_ERROR(ctx, "Index out of bounds for buffer (length %llu, index %llu)",
                buffPkgStrObj->buffer.length, index);
            return ERR(ACPI, ILSEQ);
        }

        aml_object_t* byteField = aml_object_new();
        if (byteField == NULL)
        {
            return ERR(ACPI, NOMEM);
        }
        UNREF_DEFER(byteField);

        status = aml_buffer_field_set(byteField, buffPkgStrObj, index * 8, 8);
        if (IS_ERR(status))
        {
            return status;
        }

        status = aml_object_reference_set(result, byteField);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    break;
    case AML_STRING: // Section 19.6.63.3
    {
        if (index >= buffPkgStrObj->string.length)
        {
            AML_DEBUG_ERROR(ctx, "Index out of bounds for string (length %llu, index %llu)",
                buffPkgStrObj->string.length, index);
            return ERR(ACPI, ILSEQ);
        }

        aml_object_t* byteField = aml_object_new();
        if (byteField == NULL)
        {
            return ERR(ACPI, NOMEM);
        }
        UNREF_DEFER(byteField);

        status = aml_buffer_field_set(byteField, buffPkgStrObj, index * 8, 8);
        if (IS_ERR(status))
        {
            return status;
        }

        status = aml_object_reference_set(result, byteField);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid type, expected buffer, package or string but got '%s'",
            aml_type_to_string(buffPkgStrObj->type));
        return ERR(ACPI, ILSEQ);
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_land_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    status = aml_op_operand_operand_read(ctx, AML_LAND_OP, AML_INTEGER, &operand1, &operand2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLand structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_AND));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_lequal_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    status =
        aml_op_operand_operand_read(ctx, AML_LEQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1, &operand2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLequal structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_EQUAL));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_lgreater_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    status =
        aml_op_operand_operand_read(ctx, AML_LGREATER_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1, &operand2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLgreater structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_GREATER));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_lgreater_equal_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    status = aml_op_operand_operand_read(ctx, AML_LGREATER_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1,
        &operand2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLgreaterEqual structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_GREATER_EQUAL));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_lless_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    status =
        aml_op_operand_operand_read(ctx, AML_LLESS_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1, &operand2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLless structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_LESS));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_lless_equal_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    status = aml_op_operand_operand_read(ctx, AML_LLESS_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1,
        &operand2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLlessEqual structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_LESS_EQUAL));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_lnot_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand = NULL;
    status = aml_op_operand_read(ctx, AML_LNOT_OP, AML_INTEGER, &operand);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLnot structure");
        return status;
    }
    UNREF_DEFER(operand);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare_not(operand->integer.value));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_lnot_equal_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    status = aml_op_operand_operand_read(ctx, AML_LNOT_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1,
        &operand2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLnotEqual structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_NOT_EQUAL));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_lor_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    status = aml_op_operand_operand_read(ctx, AML_LOR_OP, AML_INTEGER, &operand1, &operand2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLor structure");
        return status;
    }
    UNREF_DEFER(operand1);
    UNREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_OR));
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_mutex_object_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* result;
    status_t status = aml_super_name_read_and_resolve(ctx, &result);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
        return status;
    }
    UNREF_DEFER(result);

    if (result->type != AML_MUTEX)
    {
        AML_DEBUG_ERROR(ctx, "Object is not a Mutex");
        return ERR(ACPI, ILSEQ);
    }

    *out = REF(result);
    return OK;
}

status_t aml_timeout_read(aml_term_list_ctx_t* ctx, uint16_t* out)
{
    status_t status = aml_word_data_read(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read WordData");
        return status;
    }

    return OK;
}

status_t aml_def_acquire_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    if (!aml_token_expect(ctx, AML_ACQUIRE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read AcquireOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* mutex = NULL;
    status = aml_mutex_object_read(ctx, &mutex);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Mutex");
        return status;
    }
    UNREF_DEFER(mutex);

    assert(mutex->type == AML_MUTEX);

    uint16_t timeout;
    status = aml_timeout_read(ctx, &timeout);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Timeout");
        return status;
    }

    clock_t clockTimeout = (timeout == 0xFFFF) ? CLOCKS_NEVER : (clock_t)timeout * (CLOCKS_PER_MS);
    // If timedout result == 1, else result == 0.
    uint64_t acquireResult = aml_mutex_acquire(&mutex->mutex.mutex, mutex->mutex.syncLevel, clockTimeout);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, acquireResult);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_to_bcd_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    status_t status = aml_op_operand_target_read(ctx, AML_TO_BCD_OP, AML_INTEGER, &operand, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToBcd structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }

    aml_uint_t bcd;
    status = aml_convert_integer_to_bcd(operand->integer.value, &bcd);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to convert integer to BCD");
        return status;
    }

    status = aml_integer_set(result, bcd);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        UNREF(result);
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_to_buffer_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    status_t status =
        aml_op_operand_target_read(ctx, AML_TO_BUFFER_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToBuffer structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = NULL;
    status = aml_convert_to_buffer(ctx->state, operand, &result);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        UNREF(result);
        return status;
    }

    *out = result; // Transfer ownership
    return OK;
}

status_t aml_def_to_decimal_string_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    status_t status = aml_op_operand_target_read(ctx, AML_TO_DECIMAL_STRING_OP, AML_INTEGER, &operand, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToDecimalString structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = NULL;
    status = aml_convert_to_decimal_string(ctx->state, operand, &result);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        UNREF(result);
        return status;
    }

    *out = result; // Transfer ownership
    return OK;
}

status_t aml_def_to_hex_string_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    status_t status = aml_op_operand_target_read(ctx, AML_TO_HEX_STRING_OP, AML_INTEGER, &operand, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToHexString structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = NULL;
    status = aml_convert_to_hex_string(ctx->state, operand, &result);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        UNREF(result);
        return status;
    }

    *out = result; // Transfer ownership
    return OK;
}

status_t aml_def_to_integer_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    status_t status =
        aml_op_operand_target_read(ctx, AML_TO_INTEGER_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToInteger structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = NULL;
    status = aml_convert_to_integer(ctx->state, operand, &result);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        UNREF(result);
        return status;
    }

    *out = result; // Transfer ownership
    return OK;
}

status_t aml_length_arg_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_def_to_string_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    if (!aml_token_expect(ctx, AML_TO_STRING_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ToStringOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_buffer_t* source = NULL;
    status = aml_term_arg_read_buffer(ctx, &source);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }
    UNREF_DEFER(source);

    aml_uint_t length;
    status = aml_length_arg_read(ctx, &length);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read LengthArg");
        return status;
    }

    aml_object_t* target = NULL;
    status = aml_target_read_and_resolve(ctx, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    length = MIN(length, source->length);
    status = aml_string_set_empty(result, length);
    if (IS_ERR(status))
    {
        return status;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        result->string.content[i] = (char)source->content[i];
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_timer_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    if (!aml_token_expect(ctx, AML_TIMER_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TimerOp");
        return ERR(ACPI, ILSEQ);
    }

    // The period of the timer is supposed to be 100ns.
    uint64_t time100ns = clock_uptime() / 100;

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status_t status = aml_integer_set(result, time100ns);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_copy_object_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* source = NULL;
    aml_object_t* destination = NULL;
    status_t status =
        aml_op_termarg_simplename_read(ctx, AML_COPY_OBJECT_OP, AML_DATA_REF_OBJECTS, &source, &destination);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefCopyObject structure");
        return status;
    }
    UNREF_DEFER(source);
    UNREF_DEFER(destination);

    status = aml_copy_object(ctx->state, source, destination);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to copy object");
        return status;
    }

    *out = REF(source);
    return OK;
}

status_t aml_data_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* result;
    status_t status = aml_term_arg_read(ctx, AML_COMPUTATIONAL_DATA_OBJECTS, &result);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    *out = result; // Transfer ownership
    return OK;
}

status_t aml_def_concat_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* source1 = NULL;
    aml_object_t* source2 = NULL;
    aml_object_t* target = NULL;
    status_t status = aml_op_data_data_target_read(ctx, AML_CONCAT_OP, &source1, &source2, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefConcat structure");
        return status;
    }
    UNREF_DEFER(source1);
    UNREF_DEFER(source2);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_concat(ctx->state, source1, source2, result);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_size_of_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* object = NULL;
    status_t status = aml_op_supername_read(ctx, AML_SIZE_OF_OP, &object);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefSizeOf structure");
        return status;
    }
    UNREF_DEFER(object);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    aml_object_t* sizeObject;
    switch (object->type)
    {
    case AML_ARG:
        sizeObject = object->arg.value;
        break;
    case AML_LOCAL:
        sizeObject = object->local.value;
        break;
    default:
        sizeObject = object;
        break;
    }

    if (sizeObject == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    uint64_t size;
    switch (sizeObject->type)
    {
    case AML_BUFFER:
        size = sizeObject->buffer.length;
        break;
    case AML_STRING:
        size = sizeObject->string.length;
        break;
    case AML_PACKAGE:
        size = sizeObject->package.length;
        break;
    default:
        return ERR(ACPI, INVAL);
    }

    status = aml_integer_set(result, size);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_ref_of_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* object = NULL;
    status_t status = aml_op_supername_read(ctx, AML_REF_OF_OP, &object);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefRefOf structure");
        return status;
    }
    UNREF_DEFER(object);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_object_reference_set(result, object);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_object_type_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    if (!aml_token_expect(ctx, AML_OBJECT_TYPE_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ObjectTypeOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_token_t token;
    aml_token_peek(ctx, &token);

    status_t status;
    aml_object_t* object;
    switch (token.num)
    {
    case AML_DEBUG_OP:
        status = aml_debug_obj_read(ctx, &object);
        break;
    case AML_REF_OF_OP:
        status = aml_def_ref_of_read(ctx, &object);
        break;
    case AML_DEREF_OF_OP:
        status = aml_def_deref_of_read(ctx, &object);
        break;
    case AML_INDEX_OP:
        status = aml_def_index_read(ctx, &object);
        break;
    default:
        status = aml_simple_name_read_and_resolve(ctx, &object);
        break;
    }

    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read object from '%s'", token.props->name);
        return status;
    }
    UNREF_DEFER(object);

    uint64_t typeNum;
    switch (object->type == AML_OBJECT_REFERENCE ? object->objectReference.target->type : object->type)
    {
    case AML_INTEGER:
        typeNum = 1;
        break;
    case AML_STRING:
        typeNum = 2;
        break;
    case AML_BUFFER:
        typeNum = 3;
        break;
    case AML_PACKAGE:
        typeNum = 4;
        break;
    case AML_FIELD_UNIT:
        typeNum = 5;
        break;
    case AML_DEVICE:
        typeNum = 6;
        break;
    case AML_EVENT:
        typeNum = 7;
        break;
    case AML_METHOD:
        typeNum = 8;
        break;
    case AML_MUTEX:
        typeNum = 9;
        break;
    case AML_OPERATION_REGION:
        typeNum = 10;
        break;
    case AML_POWER_RESOURCE:
        typeNum = 11;
        break;
    case AML_THERMAL_ZONE:
        typeNum = 13;
        break;
    case AML_BUFFER_FIELD:
        typeNum = 14;
        break;
    case AML_DEBUG_OBJECT:
        typeNum = 15;
        break;
    default:
        typeNum = 0;
        break;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    status = aml_integer_set(result, typeNum);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_reference_type_opcode_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    status_t status = OK;
    aml_object_t* result = NULL;
    switch (op.num)
    {
    case AML_REF_OF_OP:
        status = aml_def_ref_of_read(ctx, &result);
        break;
    case AML_DEREF_OF_OP:
        status = aml_def_deref_of_read(ctx, &result);
        break;
    case AML_INDEX_OP:
        status = aml_def_index_read(ctx, &result);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid opcode '%s', expected RefOfOp, DerefOfOp or IndexOp", op.props->name);
        return ERR(ACPI, ILSEQ);
    }

    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read opcode '%s'", op.props->name);
        return status;
    }

    /// I am unsure about this. But it seems that ReferenceTypeOpcodes should dereference the result if its an
    /// ObjectReference. Mainly this is based of the examples found in section 19.6.63.2 and 19.6.63.3 of the Index
    /// Operator where we can see the Store Operator storing directly to the result of an Index Operator. And this seems
    /// to line up with testing. I could not find any explicit mention of this in the spec though.
    ///
    /// @todo Stare at the spec some more.

    UNREF_DEFER(result);
    if (result->type == AML_OBJECT_REFERENCE)
    {
        aml_object_t* target = result->objectReference.target;
        if (target == NULL)
        {
            AML_DEBUG_ERROR(ctx, "Object reference is NULL");
            return ERR(ACPI, ILSEQ);
        }
        *out = REF(target);
        return OK;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_find_set_left_bit_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    status_t status = aml_op_operand_target_read(ctx, AML_FIND_SET_LEFT_BIT_OP, AML_INTEGER, &operand, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefFindSetLeftBit structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    if (operand->integer.value == 0)
    {
        status = aml_integer_set(result, 0);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    else
    {
        for (uint8_t i = 0; i < aml_integer_bit_size(); i++)
        {
            if (operand->integer.value & (1ULL << (aml_integer_bit_size() - 1 - i)))
            {
                status = aml_integer_set(result, aml_integer_bit_size() - i);
                if (IS_ERR(status))
                {
                    return status;
                }
                break;
            }
        }
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_def_find_set_right_bit_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    status_t status = aml_op_operand_target_read(ctx, AML_FIND_SET_RIGHT_BIT_OP, AML_INTEGER, &operand, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefFindSetRightBit structure");
        return status;
    }
    UNREF_DEFER(operand);
    UNREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    if (operand->integer.value == 0)
    {
        status = aml_integer_set(result, 0);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    else
    {
        for (uint8_t i = 0; i < aml_integer_bit_size(); i++)
        {
            if (operand->integer.value & (1ULL << i))
            {
                status = aml_integer_set(result, i + 1);
                if (IS_ERR(status))
                {
                    return status;
                }
                break;
            }
        }
    }

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_search_pkg_read(aml_term_list_ctx_t* ctx, aml_package_t** out)
{
    aml_package_t* pkg;
    status_t status = aml_term_arg_read_package(ctx, &pkg);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    *out = pkg; // Transfer ownership
    return OK;
}

status_t aml_match_opcode_read(aml_term_list_ctx_t* ctx, aml_match_opcode_t* out)
{
    uint8_t byteData;
    status_t status = aml_byte_data_read(ctx, &byteData);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return status;
    }

    if (byteData > AML_MATCH_MGT)
    {
        AML_DEBUG_ERROR(ctx, "Invalid MatchOpcode value %u", byteData);
        return ERR(ACPI, ILSEQ);
    }

    *out = (aml_match_opcode_t)byteData;
    return OK;
}

status_t aml_start_index_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    status_t status = aml_term_arg_read_integer(ctx, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

static bool aml_match_compare(aml_object_t* obj1, aml_object_t* obj2, aml_match_opcode_t op)
{
    switch (op)
    {
    case AML_MATCH_MTR:
        return true;
    case AML_MATCH_MEQ:
        return aml_compare(obj1, obj2, AML_COMPARE_EQUAL);
    case AML_MATCH_MLE:
        return aml_compare(obj1, obj2, AML_COMPARE_LESS_EQUAL);
    case AML_MATCH_MLT:
        return aml_compare(obj1, obj2, AML_COMPARE_LESS);
    case AML_MATCH_MGE:
        return aml_compare(obj1, obj2, AML_COMPARE_GREATER_EQUAL);
    case AML_MATCH_MGT:
        return aml_compare(obj1, obj2, AML_COMPARE_GREATER);
    default:
        // This should never happen as we validate the opcode when reading it.
        return false;
    }
}

status_t aml_def_match_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status;
    if (!aml_token_expect(ctx, AML_MATCH_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MatchOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_package_t* searchPkg = NULL;
    status = aml_search_pkg_read(ctx, &searchPkg);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SearchPkg");
        return status;
    }
    UNREF_DEFER(searchPkg);

    aml_match_opcode_t op1;
    status = aml_match_opcode_read(ctx, &op1);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Op1");
        return status;
    }

    aml_object_t* object1 = NULL;
    status = aml_operand_read(ctx, AML_INTEGER | AML_STRING | AML_BUFFER, &object1);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MatchObject1");
        return status;
    }
    UNREF_DEFER(object1);

    aml_match_opcode_t op2;
    status = aml_match_opcode_read(ctx, &op2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Op2");
        return status;
    }

    aml_object_t* object2 = NULL;
    status = aml_operand_read(ctx, AML_INTEGER | AML_STRING | AML_BUFFER, &object2);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MatchObject2");
        return status;
    }
    UNREF_DEFER(object2);

    aml_uint_t startIndex;
    status = aml_start_index_read(ctx, &startIndex);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read StartIndex");
        return status;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    for (uint64_t i = startIndex; i < searchPkg->length; i++)
    {
        aml_object_t* element = searchPkg->elements[i];
        if (element == NULL)
        {
            continue;
        }

        if (element->type == AML_UNINITIALIZED)
        {
            continue;
        }

        aml_object_t* convertedFor1 = NULL;
        if (IS_ERR(aml_convert_source(ctx->state, element, &convertedFor1, object1->type)))
        {
            continue;
        }
        UNREF_DEFER(convertedFor1);

        aml_object_t* convertedFor2 = NULL;
        if (IS_ERR(aml_convert_source(ctx->state, element, &convertedFor2, object2->type)))
        {
            continue;
        }
        UNREF_DEFER(convertedFor2);

        if (aml_match_compare(convertedFor1, object1, op1) && aml_match_compare(convertedFor2, object2, op2))
        {
            status = aml_integer_set(result, i);
            if (IS_ERR(status))
            {
                return status;
            }
            *out = REF(result);
            return OK;
        }
    }

    status = aml_integer_set(result, aml_integer_ones());
    if (IS_ERR(status))
    {
        return status;
    }
    *out = REF(result);
    return OK;
}

status_t aml_mid_obj_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    status_t status = aml_term_arg_read(ctx, AML_STRING | AML_BUFFER, out);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return status;
    }

    return OK;
}

status_t aml_def_mid_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    if (!aml_token_expect(ctx, AML_MID_OP))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MidOp");
        return ERR(ACPI, ILSEQ);
    }

    aml_object_t* midObj = NULL;
    status_t status = aml_mid_obj_read(ctx, &midObj);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MidObj");
        return status;
    }
    UNREF_DEFER(midObj);

    aml_uint_t index;
    status = aml_term_arg_read_integer(ctx, &index);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Index");
        return status;
    }

    aml_uint_t length;
    status = aml_term_arg_read_integer(ctx, &length);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Length");
        return status;
    }

    aml_object_t* target = NULL;
    status = aml_target_read_and_resolve(ctx, &target);
    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return status;
    }
    UNREF_DEFER(target);

    aml_object_t* result = NULL;
    status = aml_mid(ctx->state, midObj, index, length, &result);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(result);

    status = aml_store(ctx->state, result, target);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = REF(result);
    return OK;
}

status_t aml_expression_opcode_read(aml_term_list_ctx_t* ctx, aml_object_t** out)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    aml_object_t* result = NULL;
    status_t status = OK;
    if (op.props->type == AML_TOKEN_TYPE_NAME)
    {
        // Note that just resolving an object does not set the implicit return value.
        // So we only set the implicit return value if the resolved object in MethodInvocation is a method.
        // Or one of the other expression opcodes is used.

        status = aml_method_invocation_read(ctx, &result);
        if (IS_ERR(status))
            result = NULL;
    }
    else
    {
        switch (op.num)
        {
        case AML_BUFFER_OP:
        {
            result = aml_object_new();
            if (result == NULL)
            {
                return ERR(ACPI, NOMEM);
            }

            status = aml_def_buffer_read(ctx, result);
            if (IS_ERR(status))
            {
                UNREF(result);
                AML_DEBUG_ERROR(ctx, "Failed to read opcode 'DefBuffer'");
                return status;
            }
        }
        break;
        case AML_PACKAGE_OP:
        {
            result = aml_object_new();
            if (result == NULL)
            {
                return ERR(ACPI, NOMEM);
            }

            status = aml_def_package_read(ctx, result);
            if (IS_ERR(status))
            {
                UNREF(result);
                AML_DEBUG_ERROR(ctx, "Failed to read opcode 'DefPackage'");
                return status;
            }
        }
        break;
        case AML_VAR_PACKAGE_OP:
        {
            result = aml_object_new();
            if (result == NULL)
            {
                return ERR(ACPI, NOMEM);
            }

            status = aml_def_var_package_read(ctx, result);
            if (IS_ERR(status))
            {
                UNREF(result);
                AML_DEBUG_ERROR(ctx, "Failed to read opcode 'DefVarPackage'");
                return status;
            }
        }
        break;
        case AML_COND_REF_OF_OP:
            status = aml_def_cond_ref_of_read(ctx, &result);
            break;
        case AML_STORE_OP:
            status = aml_def_store_read(ctx, &result);
            break;
        case AML_ADD_OP:
            status = aml_def_add_read(ctx, &result);
            break;
        case AML_SUBTRACT_OP:
            status = aml_def_subtract_read(ctx, &result);
            break;
        case AML_MULTIPLY_OP:
            status = aml_def_multiply_read(ctx, &result);
            break;
        case AML_DIVIDE_OP:
            status = aml_def_divide_read(ctx, &result);
            break;
        case AML_MOD_OP:
            status = aml_def_mod_read(ctx, &result);
            break;
        case AML_AND_OP:
            status = aml_def_and_read(ctx, &result);
            break;
        case AML_NAND_OP:
            status = aml_def_nand_read(ctx, &result);
            break;
        case AML_OR_OP:
            status = aml_def_or_read(ctx, &result);
            break;
        case AML_NOR_OP:
            status = aml_def_nor_read(ctx, &result);
            break;
        case AML_XOR_OP:
            status = aml_def_xor_read(ctx, &result);
            break;
        case AML_NOT_OP:
            status = aml_def_not_read(ctx, &result);
            break;
        case AML_SHIFT_LEFT_OP:
            status = aml_def_shift_left_read(ctx, &result);
            break;
        case AML_SHIFT_RIGHT_OP:
            status = aml_def_shift_right_read(ctx, &result);
            break;
        case AML_INCREMENT_OP:
            status = aml_def_increment_read(ctx, &result);
            break;
        case AML_DECREMENT_OP:
            status = aml_def_decrement_read(ctx, &result);
            break;
        case AML_DEREF_OF_OP:
            status = aml_def_deref_of_read(ctx, &result);
            break;
        case AML_INDEX_OP:
            status = aml_def_index_read(ctx, &result);
            break;
        case AML_LAND_OP:
            status = aml_def_land_read(ctx, &result);
            break;
        case AML_LEQUAL_OP:
            status = aml_def_lequal_read(ctx, &result);
            break;
        case AML_LGREATER_OP:
            status = aml_def_lgreater_read(ctx, &result);
            break;
        case AML_LGREATER_EQUAL_OP:
            status = aml_def_lgreater_equal_read(ctx, &result);
            break;
        case AML_LLESS_OP:
            status = aml_def_lless_read(ctx, &result);
            break;
        case AML_LLESS_EQUAL_OP:
            status = aml_def_lless_equal_read(ctx, &result);
            break;
        case AML_LNOT_OP:
            status = aml_def_lnot_read(ctx, &result);
            break;
        case AML_LNOT_EQUAL_OP:
            status = aml_def_lnot_equal_read(ctx, &result);
            break;
        case AML_LOR_OP:
            status = aml_def_lor_read(ctx, &result);
            break;
        case AML_ACQUIRE_OP:
            status = aml_def_acquire_read(ctx, &result);
            break;
        case AML_TO_BCD_OP:
            status = aml_def_to_bcd_read(ctx, &result);
            break;
        case AML_TO_BUFFER_OP:
            status = aml_def_to_buffer_read(ctx, &result);
            break;
        case AML_TO_DECIMAL_STRING_OP:
            status = aml_def_to_decimal_string_read(ctx, &result);
            break;
        case AML_TO_HEX_STRING_OP:
            status = aml_def_to_hex_string_read(ctx, &result);
            break;
        case AML_TO_INTEGER_OP:
            status = aml_def_to_integer_read(ctx, &result);
            break;
        case AML_TO_STRING_OP:
            status = aml_def_to_string_read(ctx, &result);
            break;
        case AML_TIMER_OP:
            status = aml_def_timer_read(ctx, &result);
            break;
        case AML_COPY_OBJECT_OP:
            status = aml_def_copy_object_read(ctx, &result);
            break;
        case AML_CONCAT_OP:
            status = aml_def_concat_read(ctx, &result);
            break;
        case AML_SIZE_OF_OP:
            status = aml_def_size_of_read(ctx, &result);
            break;
        case AML_REF_OF_OP:
            status = aml_def_ref_of_read(ctx, &result);
            break;
        case AML_OBJECT_TYPE_OP:
            status = aml_def_object_type_read(ctx, &result);
            break;
        case AML_FIND_SET_LEFT_BIT_OP:
            status = aml_def_find_set_left_bit_read(ctx, &result);
            break;
        case AML_FIND_SET_RIGHT_BIT_OP:
            status = aml_def_find_set_right_bit_read(ctx, &result);
            break;
        case AML_MATCH_OP:
            status = aml_def_match_read(ctx, &result);
            break;
        case AML_MID_OP:
            status = aml_def_mid_read(ctx, &result);
            break;
        default:
            AML_DEBUG_ERROR(ctx, "Unknown ExpressionOpcode '%s' (0x%04x)", op.props->name, op.num);
            return ERR(ACPI, IMPL);
        }

        if (result != NULL)
        {
            aml_state_result_set(ctx->state, result);
        }
    }

    if (IS_ERR(status))
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ExpressionOpcode '%s' (0x%04x)", op.props->name, op.num);
        return status;
    }

    *out = result; // Transfer ownership
    return OK;
}
