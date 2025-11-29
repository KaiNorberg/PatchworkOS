#include <modules/acpi/aml/encoding/expression.h>

#include <kernel/log/log.h>
#include <kernel/sched/sys_time.h>
#include <kernel/sched/timer.h>
#include <modules/acpi/aml/debug.h>
#include <modules/acpi/aml/encoding/arg.h>
#include <modules/acpi/aml/encoding/debug.h>
#include <modules/acpi/aml/encoding/package_length.h>
#include <modules/acpi/aml/encoding/term.h>
#include <modules/acpi/aml/object.h>
#include <modules/acpi/aml/runtime/compare.h>
#include <modules/acpi/aml/runtime/concat.h>
#include <modules/acpi/aml/runtime/convert.h>
#include <modules/acpi/aml/runtime/copy.h>
#include <modules/acpi/aml/runtime/method.h>
#include <modules/acpi/aml/runtime/mid.h>
#include <modules/acpi/aml/runtime/store.h>
#include <modules/acpi/aml/state.h>
#include <modules/acpi/aml/to_string.h>
#include <modules/acpi/aml/token.h>

#include <sys/proc.h>

aml_object_t* aml_operand_read(aml_term_list_ctx_t* ctx, aml_type_t allowedTypes)
{
    aml_object_t* result = aml_term_arg_read(ctx, allowedTypes);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    return result; // Transfer ownership
}

static inline uint64_t aml_op_operand_operand_target_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand1, aml_object_t** operand2, aml_object_t** target)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *operand1 = aml_operand_read(ctx, allowedTypes);
    if (*operand1 == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand1");
        return ERR;
    }

    // Operand2 must be the same type as operand1.
    *operand2 = aml_operand_read(ctx, (*operand1)->type);
    if (*operand2 == NULL)
    {
        DEREF(*operand1);
        AML_DEBUG_ERROR(ctx, "Failed to read operand2");
        return ERR;
    }

    if (aml_target_read_and_resolve(ctx, target) == ERR)
    {
        DEREF(*operand1);
        DEREF(*operand2);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_op_operand_operand_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand1, aml_object_t** operand2)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *operand1 = aml_operand_read(ctx, allowedTypes);
    if (*operand1 == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand1");
        return ERR;
    }

    // Operand2 must be the same type as operand1.
    *operand2 = aml_operand_read(ctx, (*operand1)->type);
    if (*operand2 == NULL)
    {
        DEREF(*operand1);
        AML_DEBUG_ERROR(ctx, "Failed to read operand2");
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_op_operand_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *operand = aml_operand_read(ctx, allowedTypes);
    if (*operand == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand");
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_op_operand_target_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand, aml_object_t** target)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *operand = aml_operand_read(ctx, allowedTypes);
    if (*operand == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand");
        return ERR;
    }

    if (aml_target_read_and_resolve(ctx, target) == ERR)
    {
        DEREF(*operand);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_op_operand_shiftcount_target_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** operand, aml_uint_t* shiftCount, aml_object_t** target)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *operand = aml_operand_read(ctx, allowedTypes);
    if (*operand == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read operand");
        return ERR;
    }

    if (aml_shift_count_read(ctx, shiftCount) == ERR)
    {
        DEREF(*operand);
        AML_DEBUG_ERROR(ctx, "Failed to read ShiftCount");
        return ERR;
    }

    if (aml_target_read_and_resolve(ctx, target) == ERR)
    {
        DEREF(*operand);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_op_data_data_target_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_object_t** data1, aml_object_t** data2, aml_object_t** target)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *data1 = aml_data_read(ctx);
    if (*data1 == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read data1");
        return ERR;
    }

    *data2 = aml_data_read(ctx);
    if (*data2 == NULL)
    {
        DEREF(*data1);
        AML_DEBUG_ERROR(ctx, "Failed to read data2");
        return ERR;
    }

    if (aml_target_read_and_resolve(ctx, target) == ERR)
    {
        DEREF(*data1);
        DEREF(*data2);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_op_termarg_simplename_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** termarg, aml_object_t** simplename)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *termarg = aml_term_arg_read(ctx, allowedTypes);
    if (*termarg == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    *simplename = aml_simple_name_read_and_resolve(ctx);
    if (*simplename == NULL)
    {
        DEREF(*termarg);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SimpleName");
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_op_supername_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_object_t** supername)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *supername = aml_super_name_read_and_resolve(ctx);
    if (*supername == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_op_termarg_supername_read(aml_term_list_ctx_t* ctx, aml_token_num_t expectedOp,
    aml_type_t allowedTypes, aml_object_t** termarg, aml_object_t** supername)
{
    if (aml_token_expect(ctx, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    *termarg = aml_term_arg_read(ctx, allowedTypes);
    if (*termarg == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    *supername = aml_super_name_read_and_resolve(ctx);
    if (*supername == NULL)
    {
        DEREF(*termarg);
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
        return ERR;
    }

    return 0;
}

uint64_t aml_buffer_size_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_buffer_read(aml_term_list_ctx_t* ctx, aml_object_t* out)
{
    if (aml_token_expect(ctx, AML_BUFFER_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BufferOp");
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

    aml_uint_t bufferSize;
    if (aml_buffer_size_read(ctx, &bufferSize) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BufferSize");
        return ERR;
    }

    uint64_t availableBytes = (uint64_t)(end - ctx->current);

    if (aml_buffer_set(out, ctx->current, availableBytes, bufferSize) == ERR)
    {
        return ERR;
    }

    ctx->current = end;
    return 0;
}

uint64_t aml_term_arg_list_read(aml_term_list_ctx_t* ctx, uint64_t argCount, aml_term_arg_list_t* out)
{
    if (argCount > AML_MAX_ARGS)
    {
        errno = EILSEQ;
        return ERR;
    }

    uint64_t i = 0;
    for (; i < argCount; i++)
    {
        out->args[i] = aml_term_arg_read(ctx, AML_DATA_REF_OBJECTS);
        if (out->args[i] == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                DEREF(out->args[j]);
                out->args[j] = NULL;
            }
            return ERR;
        }
    }
    out->args[i] = NULL;

    return 0;
}

aml_object_t* aml_method_invocation_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* target = aml_name_string_read_and_resolve(ctx);
    if (target == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve NameString");
        return NULL;
    }
    DEREF_DEFER(target);

    if (target->type == AML_METHOD)
    {
        aml_term_arg_list_t args = {0};
        if (aml_term_arg_list_read(ctx, target->method.methodFlags.argCount, &args) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to read method arguments");
            return NULL;
        }

        aml_object_t* result = aml_method_invoke(ctx->state, &target->method, args.args);
        if (result == NULL)
        {
            for (uint8_t i = 0; args.args[i] != NULL; i++)
            {
                DEREF(args.args[i]);
                args.args[i] = NULL;
            }
            AML_DEBUG_ERROR(ctx, "Failed to evaluate method '%s'", AML_NAME_TO_STRING(target->name));
            return NULL;
        }

        for (uint8_t i = 0; args.args[i] != NULL; i++)
        {
            DEREF(args.args[i]);
            args.args[i] = NULL;
        }

        aml_state_result_set(ctx->state, result);

        return result; // Transfer ownership
    }

    // Note that just resolving an object does not set the implicit return value.
    return REF(target);
}

aml_object_t* aml_def_cond_ref_of_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_COND_REF_OF_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read CondRefOfOp");
        return NULL;
    }

    aml_object_t* source = aml_super_name_read_and_resolve(ctx);
    if (source == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
        return NULL;
    }
    DEREF_DEFER(source);

    aml_object_t* result = NULL;
    if (aml_target_read_and_resolve(ctx, &result) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(result);

    aml_object_t* output = aml_object_new();
    if (output == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(output);

    if (source == NULL)
    {
        // Return false since the source did not resolve to an object.
        if (aml_integer_set(output, 0) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to init false integer");
            return NULL;
        }
        return REF(output);
    }

    if (result == NULL)
    {
        // Return true since source resolved to an object and result dident so we dont need to store anything.
        if (aml_integer_set(output, AML_TRUE) == ERR)
        {
            AML_DEBUG_ERROR(ctx, "Failed to init true integer");
            return NULL;
        }
        return REF(output);
    }

    // Store a reference to source in the result and return true.

    if (aml_object_reference_set(result, source) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to init ObjectReference in result");
        return NULL;
    }

    if (aml_integer_set(output, AML_TRUE) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to init true integer");
        return NULL;
    }

    return REF(output);
}

aml_object_t* aml_def_store_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* source = NULL;
    aml_object_t* destination = NULL;
    if (aml_op_termarg_supername_read(ctx, AML_STORE_OP, AML_DATA_REF_OBJECTS, &source, &destination) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefStore structure");
        return NULL;
    }
    DEREF_DEFER(source);
    DEREF_DEFER(destination);

    if (aml_store(ctx->state, source, destination) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to store source '%s' in destination '%s'", AML_NAME_TO_STRING(source->name),
            AML_NAME_TO_STRING(destination->name));
        return NULL;
    }

    return REF(source);
}

uint64_t aml_dividend_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_divisor_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

aml_object_t* aml_remainder_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* result = NULL;
    if (aml_target_read_and_resolve(ctx, &result) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return NULL;
    }

    return result; // Transfer ownership
}

aml_object_t* aml_quotient_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* result = NULL;
    if (aml_target_read_and_resolve(ctx, &result) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return NULL;
    }

    return result; // Transfer ownership
}

aml_object_t* aml_def_add_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_operand_target_read(ctx, AML_ADD_OP, AML_INTEGER, &operand1, &operand2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefAdd structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, operand1->integer.value + operand2->integer.value) ||
        aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_subtract_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_operand_target_read(ctx, AML_SUBTRACT_OP, AML_INTEGER, &operand1, &operand2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefSubtract structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, operand1->integer.value - operand2->integer.value) ||
        aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_multiply_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_operand_target_read(ctx, AML_MULTIPLY_OP, AML_INTEGER, &operand1, &operand2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefMultiply structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, operand1->integer.value * operand2->integer.value) ||
        aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_divide_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_DIVIDE_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DivideOp");
        return NULL;
    }

    aml_uint_t dividend;
    if (aml_dividend_read(ctx, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Dividend");
        return NULL;
    }

    aml_uint_t divisor;
    if (aml_divisor_read(ctx, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Divisor");
        return NULL;
    }

    if (divisor == 0)
    {
        divisor = 1;
    }

    aml_object_t* remainderDest = aml_remainder_read(ctx);
    if (remainderDest == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read remainder");
        return NULL;
    }
    DEREF_DEFER(remainderDest);

    aml_object_t* quotientDest = aml_quotient_read(ctx);
    if (quotientDest == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read quotient");
        return NULL;
    }
    DEREF_DEFER(quotientDest);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    // Init with remainder.
    if (aml_integer_set(result, dividend % divisor) == ERR || aml_store(ctx->state, result, remainderDest))
    {
        AML_DEBUG_ERROR(ctx, "Failed to store remainder");
        return NULL;
    }

    // Init with quotient.
    if (aml_integer_set(result, dividend / divisor) == ERR || aml_store(ctx->state, result, quotientDest))
    {
        AML_DEBUG_ERROR(ctx, "Failed to store quotient");
        return NULL;
    }

    // Qoutient stays in result.
    return REF(result);
}

aml_object_t* aml_def_mod_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_MOD_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ModOp");
        return NULL;
    }

    aml_uint_t dividend;
    if (aml_dividend_read(ctx, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Dividend");
        return NULL;
    }

    aml_uint_t divisor;
    if (aml_divisor_read(ctx, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Divisor");
        return NULL;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(ctx, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    if (divisor == 0)
    {
        divisor = 1;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, dividend % divisor) == ERR || aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_and_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_operand_target_read(ctx, AML_AND_OP, AML_INTEGER, &operand1, &operand2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefAnd structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, operand1->integer.value & operand2->integer.value) ||
        aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_nand_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_operand_target_read(ctx, AML_NAND_OP, AML_INTEGER, &operand1, &operand2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefNand structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, ~(operand1->integer.value & operand2->integer.value)) ||
        aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_or_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_operand_target_read(ctx, AML_OR_OP, AML_INTEGER, &operand1, &operand2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefOr structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, operand1->integer.value | operand2->integer.value) ||
        aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_nor_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_operand_target_read(ctx, AML_NOR_OP, AML_INTEGER, &operand1, &operand2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefNor structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, ~(operand1->integer.value | operand2->integer.value)) ||
        aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_xor_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_operand_target_read(ctx, AML_XOR_OP, AML_INTEGER, &operand1, &operand2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefXor structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, operand1->integer.value ^ operand2->integer.value) ||
        aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_not_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_target_read(ctx, AML_NOT_OP, AML_INTEGER, &operand, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefNot structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, ~operand->integer.value) || aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

uint64_t aml_shift_count_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

aml_object_t* aml_def_shift_left_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    aml_uint_t shiftCount;
    if (aml_op_operand_shiftcount_target_read(ctx, AML_SHIFT_LEFT_OP, AML_INTEGER, &operand, &shiftCount, &target) ==
        ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefShiftLeft structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    // C will discard the most significant bits
    if (shiftCount >= aml_integer_bit_size())
    {
        if (aml_integer_set(result, 0) == ERR)
        {
            return NULL;
        }
    }
    else
    {
        aml_uint_t operandValue = operand->integer.value;
        if (aml_integer_set(result, operandValue << shiftCount) == ERR)
        {
            return NULL;
        }
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_shift_right_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    aml_uint_t shiftCount;
    if (aml_op_operand_shiftcount_target_read(ctx, AML_SHIFT_RIGHT_OP, AML_INTEGER, &operand, &shiftCount, &target) ==
        ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefShiftRight structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    // C will zero the most significant bits
    if (shiftCount >= aml_integer_bit_size())
    {
        if (aml_integer_set(result, 0) == ERR)
        {
            return NULL;
        }
    }
    else
    {
        aml_uint_t operandValue = operand->integer.value;
        if (aml_integer_set(result, operandValue >> shiftCount) == ERR)
        {
            return NULL;
        }
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_increment_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* superName = NULL;
    if (aml_op_supername_read(ctx, AML_INCREMENT_OP, &superName) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefIncrement structure");
        return NULL;
    }
    DEREF_DEFER(superName);

    aml_object_t* source = NULL;
    if (aml_convert_source(ctx->state, superName, &source, AML_INTEGER) == ERR)
    {
        return NULL;
    }
    DEREF_DEFER(source);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, source->integer.value + 1) == ERR ||
        aml_convert_result(ctx->state, result, superName) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_decrement_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* superName = NULL;
    if (aml_op_supername_read(ctx, AML_DECREMENT_OP, &superName) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefDecrement structure");
        return NULL;
    }
    DEREF_DEFER(superName);

    aml_object_t* source = NULL;
    if (aml_convert_source(ctx->state, superName, &source, AML_INTEGER) == ERR)
    {
        return NULL;
    }
    DEREF_DEFER(source);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, source->integer.value - 1) == ERR ||
        aml_convert_result(ctx->state, result, superName) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_obj_reference_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* termArg = aml_term_arg_read(ctx, AML_OBJECT_REFERENCE | AML_STRING);
    if (termArg == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }
    DEREF_DEFER(termArg);

    if (termArg->type == AML_OBJECT_REFERENCE)
    {
        return REF(termArg->objectReference.target);
    }
    else if (termArg->type == AML_STRING)
    {
        aml_object_t* target = aml_namespace_find_by_path(&ctx->state->overlay, ctx->scope, termArg->string.content);
        if (target == NULL)
        {
            AML_DEBUG_ERROR(ctx, "Failed to find target scope '%s'", termArg->string.content);
            errno = EILSEQ;
            return NULL;
        }

        return target; // Transfer ownership
    }

    // Should never happen.
    errno = EILSEQ;
    return NULL;
}

aml_object_t* aml_def_deref_of_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_DEREF_OF_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DerefOfOp");
        return NULL;
    }

    aml_object_t* obj = aml_obj_reference_read(ctx);
    if (obj == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ObjReference");
        return NULL;
    }

    return obj; // Transfer ownership
}

aml_object_t* aml_buff_pkg_str_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* result = aml_term_arg_read(ctx, AML_BUFFER | AML_PACKAGE | AML_STRING);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    return result; // Transfer ownership
}

uint64_t aml_index_value_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

aml_object_t* aml_def_index_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_INDEX_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read IndexOp");
        return NULL;
    }

    aml_object_t* buffPkgStrObj = aml_buff_pkg_str_obj_read(ctx);
    if (buffPkgStrObj == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read BuffPkgStrObj");
        return NULL;
    }
    DEREF_DEFER(buffPkgStrObj);

    aml_uint_t index;
    if (aml_index_value_read(ctx, &index) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read IndexValue");
        return NULL;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(ctx, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    switch (buffPkgStrObj->type)
    {
    case AML_PACKAGE: // Section 19.6.63.1
    {
        aml_package_t* package = &buffPkgStrObj->package;

        if (index >= package->length)
        {
            AML_DEBUG_ERROR(ctx, "Index out of bounds for package (length %llu, index %llu)", package->length, index);
            errno = EILSEQ;
            return NULL;
        }

        if (aml_object_reference_set(result, package->elements[index]) == ERR)
        {
            return NULL;
        }
    }
    break;
    case AML_BUFFER: // Section 19.6.63.2
    {
        if (index >= buffPkgStrObj->buffer.length)
        {
            AML_DEBUG_ERROR(ctx, "Index out of bounds for buffer (length %llu, index %llu)",
                buffPkgStrObj->buffer.length, index);
            errno = EILSEQ;
            return NULL;
        }

        aml_object_t* byteField = aml_object_new();
        if (byteField == NULL)
        {
            return NULL;
        }
        DEREF_DEFER(byteField);

        if (aml_buffer_field_set(byteField, buffPkgStrObj, index * 8, 8) == ERR)
        {
            return NULL;
        }

        if (aml_object_reference_set(result, byteField) == ERR)
        {
            return NULL;
        }
    }
    break;
    case AML_STRING: // Section 19.6.63.3
    {
        if (index >= buffPkgStrObj->string.length)
        {
            AML_DEBUG_ERROR(ctx, "Index out of bounds for string (length %llu, index %llu)",
                buffPkgStrObj->string.length, index);
            errno = EILSEQ;
            return NULL;
        }

        aml_object_t* byteField = aml_object_new();
        if (byteField == NULL)
        {
            return NULL;
        }
        DEREF_DEFER(byteField);

        if (aml_buffer_field_set(byteField, buffPkgStrObj, index * 8, 8) == ERR)
        {
            return NULL;
        }

        if (aml_object_reference_set(result, byteField) == ERR)
        {
            return NULL;
        }
    }
    break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid type, expected buffer, package or string but got '%s'",
            aml_type_to_string(buffPkgStrObj->type));
        errno = EILSEQ;
        return NULL;
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_land_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    if (aml_op_operand_operand_read(ctx, AML_LAND_OP, AML_INTEGER, &operand1, &operand2) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLand structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_AND)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_lequal_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    if (aml_op_operand_operand_read(ctx, AML_LEQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1, &operand2) ==
        ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLequal structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_EQUAL)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_lgreater_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    if (aml_op_operand_operand_read(ctx, AML_LGREATER_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1,
            &operand2) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLgreater structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_GREATER)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_lgreater_equal_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    if (aml_op_operand_operand_read(ctx, AML_LGREATER_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1,
            &operand2) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLgreaterEqual structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_GREATER_EQUAL)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_lless_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    if (aml_op_operand_operand_read(ctx, AML_LLESS_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1, &operand2) ==
        ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLless structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_LESS)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_lless_equal_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    if (aml_op_operand_operand_read(ctx, AML_LLESS_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1,
            &operand2) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLlessEqual structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_LESS_EQUAL)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_lnot_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    if (aml_op_operand_read(ctx, AML_LNOT_OP, AML_INTEGER, &operand) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLnot structure");
        return NULL;
    }
    DEREF_DEFER(operand);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare_not(operand->integer.value)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_lnot_equal_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    if (aml_op_operand_operand_read(ctx, AML_LNOT_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand1,
            &operand2) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLnotEqual structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_NOT_EQUAL)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_lor_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand1 = NULL;
    aml_object_t* operand2 = NULL;
    if (aml_op_operand_operand_read(ctx, AML_LOR_OP, AML_INTEGER, &operand1, &operand2) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefLor structure");
        return NULL;
    }
    DEREF_DEFER(operand1);
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, aml_compare(operand1, operand2, AML_COMPARE_OR)) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_mutex_object_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* result = aml_super_name_read_and_resolve(ctx);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve SuperName");
        return NULL;
    }
    DEREF_DEFER(result);

    if (result->type != AML_MUTEX)
    {
        AML_DEBUG_ERROR(ctx, "Object is not a Mutex");
        errno = EILSEQ;
        return NULL;
    }

    return REF(result);
}

uint64_t aml_timeout_read(aml_term_list_ctx_t* ctx, uint16_t* out)
{
    if (aml_word_data_read(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read WordData");
        return ERR;
    }

    return 0;
}

aml_object_t* aml_def_acquire_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_ACQUIRE_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read AcquireOp");
        return NULL;
    }

    aml_object_t* mutex = aml_mutex_object_read(ctx);
    if (mutex == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Mutex");
        return NULL;
    }
    DEREF_DEFER(mutex);

    assert(mutex->type == AML_MUTEX);

    uint16_t timeout;
    if (aml_timeout_read(ctx, &timeout) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Timeout");
        return NULL;
    }

    clock_t clockTimeout = (timeout == 0xFFFF) ? CLOCKS_NEVER : (clock_t)timeout * (CLOCKS_PER_SEC / 1000);
    // If timedout result == 1, else result == 0.
    uint64_t acquireResult = aml_mutex_acquire(&mutex->mutex.mutex, mutex->mutex.syncLevel, clockTimeout);
    if (acquireResult == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to acquire mutex");
        return NULL;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, acquireResult) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_to_bcd_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_target_read(ctx, AML_TO_BCD_OP, AML_INTEGER, &operand, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToBcd structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }

    aml_uint_t bcd;
    if (aml_convert_integer_to_bcd(operand->integer.value, &bcd) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to convert integer to BCD");
        return NULL;
    }

    if (aml_integer_set(result, bcd) == ERR || aml_store(ctx->state, result, target) == ERR)
    {
        DEREF(result);
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_to_buffer_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_target_read(ctx, AML_TO_BUFFER_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand, &target) ==
        ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToBuffer structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = NULL;
    if (aml_convert_to_buffer(ctx->state, operand, &result) == ERR)
    {
        return NULL;
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        DEREF(result);
        return NULL;
    }

    return result; // Transfer ownership
}

aml_object_t* aml_def_to_decimal_string_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_target_read(ctx, AML_TO_DECIMAL_STRING_OP, AML_INTEGER, &operand, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToDecimalString structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = NULL;
    if (aml_convert_to_decimal_string(ctx->state, operand, &result) == ERR)
    {
        return NULL;
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        DEREF(result);
        return NULL;
    }

    return result; // Transfer ownership
}

aml_object_t* aml_def_to_hex_string_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_target_read(ctx, AML_TO_HEX_STRING_OP, AML_INTEGER, &operand, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToHexString structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = NULL;
    if (aml_convert_to_hex_string(ctx->state, operand, &result) == ERR)
    {
        return NULL;
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        DEREF(result);
        return NULL;
    }

    return result; // Transfer ownership
}

aml_object_t* aml_def_to_integer_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_target_read(ctx, AML_TO_INTEGER_OP, AML_INTEGER | AML_STRING | AML_BUFFER, &operand, &target) ==
        ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefToInteger structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = NULL;
    if (aml_convert_to_integer(ctx->state, operand, &result) == ERR)
    {
        return NULL;
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        DEREF(result);
        return NULL;
    }

    return result; // Transfer ownership
}

uint64_t aml_length_arg_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

aml_object_t* aml_def_to_string_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_TO_STRING_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ToStringOp");
        return NULL;
    }

    aml_buffer_t* source = aml_term_arg_read_buffer(ctx);
    if (source == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }
    DEREF_DEFER(source);

    aml_uint_t length;
    if (aml_length_arg_read(ctx, &length) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read LengthArg");
        return NULL;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(ctx, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    length = MIN(length, source->length);
    if (aml_string_set_empty(result, length) == ERR)
    {
        return NULL;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        result->string.content[i] = (char)source->content[i];
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_timer_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_TIMER_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TimerOp");
        return NULL;
    }

    // The period of the timer is supposed to be 100ns.
    uint64_t time100ns = sys_time_uptime() / 100;

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, time100ns) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_copy_object_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* source = NULL;
    aml_object_t* destination = NULL;
    if (aml_op_termarg_simplename_read(ctx, AML_COPY_OBJECT_OP, AML_DATA_REF_OBJECTS, &source, &destination) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefCopyObject structure");
        return NULL;
    }
    DEREF_DEFER(source);
    DEREF_DEFER(destination);

    if (aml_copy_object(ctx->state, source, destination) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to copy object");
        return NULL;
    }

    return REF(source);
}

aml_object_t* aml_data_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* result = aml_term_arg_read(ctx, AML_COMPUTATIONAL_DATA_OBJECTS);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    return result; // Transfer ownership
}

aml_object_t* aml_def_concat_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* source1 = NULL;
    aml_object_t* source2 = NULL;
    aml_object_t* target = NULL;
    if (aml_op_data_data_target_read(ctx, AML_CONCAT_OP, &source1, &source2, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefConcat structure");
        return NULL;
    }
    DEREF_DEFER(source1);
    DEREF_DEFER(source2);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_concat(ctx->state, source1, source2, result) == ERR || aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_size_of_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* object = NULL;
    if (aml_op_supername_read(ctx, AML_SIZE_OF_OP, &object) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefSizeOf structure");
        return NULL;
    }
    DEREF_DEFER(object);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

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
        errno = EINVAL;
        return NULL;
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
        errno = EINVAL;
        return NULL;
    }

    if (aml_integer_set(result, size) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_ref_of_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* object = NULL;
    if (aml_op_supername_read(ctx, AML_REF_OF_OP, &object) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefRefOf structure");
        return NULL;
    }
    DEREF_DEFER(object);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_object_reference_set(result, object) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_object_type_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_OBJECT_TYPE_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ObjectTypeOp");
        return NULL;
    }

    aml_token_t token;
    aml_token_peek(ctx, &token);

    aml_object_t* object;
    switch (token.num)
    {
    case AML_DEBUG_OP:
        object = aml_debug_obj_read(ctx);
        break;
    case AML_REF_OF_OP:
        object = aml_def_ref_of_read(ctx);
        break;
    case AML_DEREF_OF_OP:
        object = aml_def_deref_of_read(ctx);
        break;
    case AML_INDEX_OP:
        object = aml_def_index_read(ctx);
        break;
    default:
        object = aml_simple_name_read_and_resolve(ctx);
        break;
    }

    if (object == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read object from '%s'", token.props->name);
        return NULL;
    }
    DEREF_DEFER(object);

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
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_set(result, typeNum) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_reference_type_opcode_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    aml_object_t* result = NULL;
    switch (op.num)
    {
    case AML_REF_OF_OP:
        result = aml_def_ref_of_read(ctx);
        break;
    case AML_DEREF_OF_OP:
        result = aml_def_deref_of_read(ctx);
        break;
    case AML_INDEX_OP:
        result = aml_def_index_read(ctx);
        break;
    default:
        AML_DEBUG_ERROR(ctx, "Invalid opcode '%s', expected RefOfOp, DerefOfOp or IndexOp", op.props->name);
        errno = EILSEQ;
        return NULL;
    }

    if (result == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read opcode '%s'", op.props->name);
        return NULL;
    }

    // I am unsure about this. But it seems that ReferenceTypeOpcodes should dereference the result if its an
    // ObjectReference. Mainly this is based of the examples found in section 19.6.63.2 and 19.6.63.3 of the Index
    // Operator where we can see the Store Operator storing directly to the result of an Index Operator. And this seems
    // to line up with testing. I could not find any explicit mention of this in the spec though.
    //
    // TODO: Stare at the spec some more.

    DEREF_DEFER(result);
    if (result->type == AML_OBJECT_REFERENCE)
    {
        aml_object_t* target = result->objectReference.target;
        if (target == NULL)
        {
            AML_DEBUG_ERROR(ctx, "Object reference is NULL");
            errno = EILSEQ;
            return NULL;
        }
        return REF(target);
    }

    return REF(result);
}

aml_object_t* aml_def_find_set_left_bit_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_target_read(ctx, AML_FIND_SET_LEFT_BIT_OP, AML_INTEGER, &operand, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefFindSetLeftBit structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (operand->integer.value == 0)
    {
        if (aml_integer_set(result, 0) == ERR)
        {
            return NULL;
        }
    }
    else
    {
        for (uint8_t i = 0; i < aml_integer_bit_size(); i++)
        {
            if (operand->integer.value & (1ULL << (aml_integer_bit_size() - 1 - i)))
            {
                if (aml_integer_set(result, aml_integer_bit_size() - i) == ERR)
                {
                    return NULL;
                }
                break;
            }
        }
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_def_find_set_right_bit_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* operand = NULL;
    aml_object_t* target = NULL;
    if (aml_op_operand_target_read(ctx, AML_FIND_SET_RIGHT_BIT_OP, AML_INTEGER, &operand, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read DefFindSetRightBit structure");
        return NULL;
    }
    DEREF_DEFER(operand);
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (operand->integer.value == 0)
    {
        if (aml_integer_set(result, 0) == ERR)
        {
            return NULL;
        }
    }
    else
    {
        for (uint8_t i = 0; i < aml_integer_bit_size(); i++)
        {
            if (operand->integer.value & (1ULL << i))
            {
                if (aml_integer_set(result, i + 1) == ERR)
                {
                    return NULL;
                }
                break;
            }
        }
    }

    if (aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_package_t* aml_search_pkg_read(aml_term_list_ctx_t* ctx)
{
    aml_package_t* pkg = aml_term_arg_read_package(ctx);
    if (pkg == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    return pkg; // Transfer ownership
}

uint64_t aml_match_opcode_read(aml_term_list_ctx_t* ctx, aml_match_opcode_t* out)
{
    uint8_t byteData;
    if (aml_byte_data_read(ctx, &byteData) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ByteData");
        return ERR;
    }

    if (byteData > AML_MATCH_MGT)
    {
        AML_DEBUG_ERROR(ctx, "Invalid MatchOpcode value %u", byteData);
        errno = EILSEQ;
        return ERR;
    }

    *out = (aml_match_opcode_t)byteData;
    return 0;
}

uint64_t aml_start_index_read(aml_term_list_ctx_t* ctx, aml_uint_t* out)
{
    if (aml_term_arg_read_integer(ctx, out) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return ERR;
    }

    return 0;
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

aml_object_t* aml_def_match_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_MATCH_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MatchOp");
        return NULL;
    }

    aml_package_t* searchPkg = aml_search_pkg_read(ctx);
    if (searchPkg == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read SearchPkg");
        return NULL;
    }
    DEREF_DEFER(searchPkg);

    aml_match_opcode_t op1;
    if (aml_match_opcode_read(ctx, &op1) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Op1");
        return NULL;
    }

    aml_object_t* object1 = aml_operand_read(ctx, AML_INTEGER | AML_STRING | AML_BUFFER);
    if (object1 == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MatchObject1");
        return NULL;
    }
    DEREF_DEFER(object1);

    aml_match_opcode_t op2;
    if (aml_match_opcode_read(ctx, &op2) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Op2");
        return NULL;
    }

    aml_object_t* object2 = aml_operand_read(ctx, AML_INTEGER | AML_STRING | AML_BUFFER);
    if (object2 == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MatchObject2");
        return NULL;
    }
    DEREF_DEFER(object2);

    aml_uint_t startIndex;
    if (aml_start_index_read(ctx, &startIndex) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read StartIndex");
        return NULL;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

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
        if (aml_convert_source(ctx->state, element, &convertedFor1, object1->type) == ERR)
        {
            errno = EOK;
            continue;
        }
        DEREF_DEFER(convertedFor1);

        aml_object_t* convertedFor2 = NULL;
        if (aml_convert_source(ctx->state, element, &convertedFor2, object2->type) == ERR)
        {
            errno = EOK;
            continue;
        }
        DEREF_DEFER(convertedFor2);

        if (aml_match_compare(convertedFor1, object1, op1) && aml_match_compare(convertedFor2, object2, op2))
        {
            if (aml_integer_set(result, i) == ERR)
            {
                return NULL;
            }
            return REF(result);
        }
    }

    if (aml_integer_set(result, aml_integer_ones()) == ERR)
    {
        return NULL;
    }
    return REF(result);
}

aml_object_t* aml_mid_obj_read(aml_term_list_ctx_t* ctx)
{
    aml_object_t* result = aml_term_arg_read(ctx, AML_STRING | AML_BUFFER);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read TermArg");
        return NULL;
    }

    return result; // Transfer ownership
}

aml_object_t* aml_def_mid_read(aml_term_list_ctx_t* ctx)
{
    if (aml_token_expect(ctx, AML_MID_OP) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MidOp");
        return NULL;
    }

    aml_object_t* midObj = aml_mid_obj_read(ctx);
    if (midObj == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read MidObj");
        return NULL;
    }
    DEREF_DEFER(midObj);

    aml_uint_t index;
    if (aml_term_arg_read_integer(ctx, &index) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Index");
        return NULL;
    }

    aml_uint_t length;
    if (aml_term_arg_read_integer(ctx, &length) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read Length");
        return NULL;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(ctx, &target) == ERR)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    aml_object_t* result = aml_mid(ctx->state, midObj, index, length);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_store(ctx->state, result, target) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

aml_object_t* aml_expression_opcode_read(aml_term_list_ctx_t* ctx)
{
    aml_token_t op;
    aml_token_peek(ctx, &op);

    aml_object_t* result = NULL;
    if (op.props->type == AML_TOKEN_TYPE_NAME)
    {
        // Note that just resolving an object does not set the implicit return value.
        // So we only set the implicit return value if the resolved object in MethodInvocation is a method.
        // Or one of the other expression opcodes is used.

        result = aml_method_invocation_read(ctx);
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
                return NULL;
            }

            if (aml_def_buffer_read(ctx, result) == ERR)
            {
                DEREF(result);
                AML_DEBUG_ERROR(ctx, "Failed to read opcode 'DefBuffer'");
                return NULL;
            }
        }
        break;
        case AML_PACKAGE_OP:
        {
            result = aml_object_new();
            if (result == NULL)
            {
                return NULL;
            }

            if (aml_def_package_read(ctx, result) == ERR)
            {
                DEREF(result);
                AML_DEBUG_ERROR(ctx, "Failed to read opcode 'DefPackage'");
                return NULL;
            }
        }
        break;
        case AML_VAR_PACKAGE_OP:
        {
            result = aml_object_new();
            if (result == NULL)
            {
                return NULL;
            }

            if (aml_def_var_package_read(ctx, result) == ERR)
            {
                DEREF(result);
                AML_DEBUG_ERROR(ctx, "Failed to read opcode 'DefVarPackage'");
                return NULL;
            }
        }
        break;
        case AML_COND_REF_OF_OP:
            result = aml_def_cond_ref_of_read(ctx);
            break;
        case AML_STORE_OP:
            result = aml_def_store_read(ctx);
            break;
        case AML_ADD_OP:
            result = aml_def_add_read(ctx);
            break;
        case AML_SUBTRACT_OP:
            result = aml_def_subtract_read(ctx);
            break;
        case AML_MULTIPLY_OP:
            result = aml_def_multiply_read(ctx);
            break;
        case AML_DIVIDE_OP:
            result = aml_def_divide_read(ctx);
            break;
        case AML_MOD_OP:
            result = aml_def_mod_read(ctx);
            break;
        case AML_AND_OP:
            result = aml_def_and_read(ctx);
            break;
        case AML_NAND_OP:
            result = aml_def_nand_read(ctx);
            break;
        case AML_OR_OP:
            result = aml_def_or_read(ctx);
            break;
        case AML_NOR_OP:
            result = aml_def_nor_read(ctx);
            break;
        case AML_XOR_OP:
            result = aml_def_xor_read(ctx);
            break;
        case AML_NOT_OP:
            result = aml_def_not_read(ctx);
            break;
        case AML_SHIFT_LEFT_OP:
            result = aml_def_shift_left_read(ctx);
            break;
        case AML_SHIFT_RIGHT_OP:
            result = aml_def_shift_right_read(ctx);
            break;
        case AML_INCREMENT_OP:
            result = aml_def_increment_read(ctx);
            break;
        case AML_DECREMENT_OP:
            result = aml_def_decrement_read(ctx);
            break;
        case AML_DEREF_OF_OP:
            result = aml_def_deref_of_read(ctx);
            break;
        case AML_INDEX_OP:
            result = aml_def_index_read(ctx);
            break;
        case AML_LAND_OP:
            result = aml_def_land_read(ctx);
            break;
        case AML_LEQUAL_OP:
            result = aml_def_lequal_read(ctx);
            break;
        case AML_LGREATER_OP:
            result = aml_def_lgreater_read(ctx);
            break;
        case AML_LGREATER_EQUAL_OP:
            result = aml_def_lgreater_equal_read(ctx);
            break;
        case AML_LLESS_OP:
            result = aml_def_lless_read(ctx);
            break;
        case AML_LLESS_EQUAL_OP:
            result = aml_def_lless_equal_read(ctx);
            break;
        case AML_LNOT_OP:
            result = aml_def_lnot_read(ctx);
            break;
        case AML_LNOT_EQUAL_OP:
            result = aml_def_lnot_equal_read(ctx);
            break;
        case AML_LOR_OP:
            result = aml_def_lor_read(ctx);
            break;
        case AML_ACQUIRE_OP:
            result = aml_def_acquire_read(ctx);
            break;
        case AML_TO_BCD_OP:
            result = aml_def_to_bcd_read(ctx);
            break;
        case AML_TO_BUFFER_OP:
            result = aml_def_to_buffer_read(ctx);
            break;
        case AML_TO_DECIMAL_STRING_OP:
            result = aml_def_to_decimal_string_read(ctx);
            break;
        case AML_TO_HEX_STRING_OP:
            result = aml_def_to_hex_string_read(ctx);
            break;
        case AML_TO_INTEGER_OP:
            result = aml_def_to_integer_read(ctx);
            break;
        case AML_TO_STRING_OP:
            result = aml_def_to_string_read(ctx);
            break;
        case AML_TIMER_OP:
            result = aml_def_timer_read(ctx);
            break;
        case AML_COPY_OBJECT_OP:
            result = aml_def_copy_object_read(ctx);
            break;
        case AML_CONCAT_OP:
            result = aml_def_concat_read(ctx);
            break;
        case AML_SIZE_OF_OP:
            result = aml_def_size_of_read(ctx);
            break;
        case AML_REF_OF_OP:
            result = aml_def_ref_of_read(ctx);
            break;
        case AML_OBJECT_TYPE_OP:
            result = aml_def_object_type_read(ctx);
            break;
        case AML_FIND_SET_LEFT_BIT_OP:
            result = aml_def_find_set_left_bit_read(ctx);
            break;
        case AML_FIND_SET_RIGHT_BIT_OP:
            result = aml_def_find_set_right_bit_read(ctx);
            break;
        case AML_MATCH_OP:
            result = aml_def_match_read(ctx);
            break;
        case AML_MID_OP:
            result = aml_def_mid_read(ctx);
            break;
        default:
            AML_DEBUG_ERROR(ctx, "Unknown ExpressionOpcode '%s' (0x%04x)", op.props->name, op.num);
            errno = ENOSYS;
            return NULL;
        }

        if (result != NULL)
        {
            aml_state_result_set(ctx->state, result);
        }
    }

    if (result == NULL)
    {
        AML_DEBUG_ERROR(ctx, "Failed to read ExpressionOpcode '%s' (0x%04x)", op.props->name, op.num);
        return NULL;
    }

    return result; // Transfer ownership
}
