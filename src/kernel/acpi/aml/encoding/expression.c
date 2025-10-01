#include "expression.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_object.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_token.h"
#include "acpi/aml/runtime/compare.h"
#include "acpi/aml/runtime/convert.h"
#include "acpi/aml/runtime/method.h"
#include "arg.h"
#include "package_length.h"
#include "term.h"

#include <sys/proc.h>

// TODO: Write helper functions to reduce code duplication.

uint64_t aml_buffer_size_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_buffer_read(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    aml_token_t bufferOp;
    if (aml_token_read(state, &bufferOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BufferOp");
        return ERR;
    }

    if (bufferOp.num != AML_BUFFER_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid BufferOp '0x%x'", bufferOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read PkgLength");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    uint64_t bufferSize;
    if (aml_buffer_size_read(state, scope, &bufferSize) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BufferSize");
        return ERR;
    }

    uint64_t availableBytes = (uint64_t)(end - state->current);

    if (aml_object_init_buffer(out, state->current, availableBytes, bufferSize) == ERR)
    {
        return ERR;
    }

    aml_state_advance(state, availableBytes);
    return 0;
}

uint64_t aml_term_arg_list_read(aml_state_t* state, aml_scope_t* scope, uint64_t argCount, aml_term_arg_list_t* out)
{
    if (argCount > AML_MAX_ARGS)
    {
        errno = EILSEQ;
        return ERR;
    }

    out->count = 0;
    for (uint64_t i = 0; i < argCount; i++)
    {
        out->args[i] = NULL;
        if (aml_term_arg_read(state, scope, &out->args[i], AML_DATA_ALL) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_object_deinit(out->args[i]);
            }
            return ERR;
        }

        out->count++;
    }

    return 0;
}

uint64_t aml_method_invocation_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_object_t* target = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve NameString");
        return ERR;
    }

    if (target->type == AML_DATA_METHOD)
    {
        aml_term_arg_list_t args = {0};
        if (aml_term_arg_list_read(state, scope, target->method.flags.argCount, &args) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read method arguments");
            return ERR;
        }

        *out = aml_scope_get_temp(scope);
        if (*out == NULL)
        {
            return ERR;
        }

        if (aml_method_evaluate(target, &args, *out) == ERR)
        {
            for (uint64_t i = 0; i < args.count; i++)
            {
                aml_object_deinit(args.args[i]);
            }
            return ERR;
        }

        for (uint64_t i = 0; i < args.count; i++)
        {
            aml_object_deinit(args.args[i]);
        }

        return 0;
    }

    *out = target;
    return 0;
}

uint64_t aml_def_cond_ref_of_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t condRefOfOp;
    if (aml_token_read(state, &condRefOfOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read CondRefOfOp");
        return ERR;
    }

    if (condRefOfOp.num != AML_COND_REF_OF_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid CondRefOfOp '0x%x'", condRefOfOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* source = NULL;
    if (aml_super_name_read_and_resolve(state, scope, &source, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    aml_object_t* result = NULL;
    if (aml_target_read_and_resolve(state, scope, &result, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (source == NULL)
    {
        // Return false since the source did not resolve to an object.
        if (aml_object_init_integer(*out, 0) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init false integer");
            return ERR;
        }
        return 0;
    }

    if (result == NULL)
    {
        // Return true since source resolved to an object and result dident so we dont need to store anything.
        if (aml_object_init_integer(*out, 1) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init true integer");
            return ERR;
        }
        return 0;
    }

    // Store a reference to source in the result and return true.

    if (aml_object_init_object_reference(result, source) == ERR)
    {
        aml_object_deinit(*out);
        AML_DEBUG_ERROR(state, "Failed to init ObjectReference in result");
        return ERR;
    }

    if (aml_object_init_integer(*out, 1) == ERR)
    {
        aml_object_deinit(*out);
        AML_DEBUG_ERROR(state, "Failed to init true integer");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_store_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t storeOp;
    if (aml_token_read(state, &storeOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read StoreOp");
        return ERR;
    }

    if (storeOp.num != AML_STORE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid StoreOp '0x%x'", storeOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* result = NULL;
    if (aml_term_arg_read(state, scope, &result, AML_DATA_ALL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    aml_object_t* destination = NULL;
    if (aml_super_name_read_and_resolve(state, scope, &destination, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    if (aml_convert_result(result, destination) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert result '%.*s' of type '%s' to destination '%.*s' of type '%s'",
            AML_NAME_LENGTH, result->segment, aml_data_type_to_string(result->type), AML_NAME_LENGTH,
            destination->segment, aml_data_type_to_string(destination->type));
        return ERR;
    }

    *out = destination;
    return 0;
}

uint64_t aml_operand_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out, aml_data_type_t allowedTypes)
{
    if (aml_term_arg_read(state, scope, out, allowedTypes) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_dividend_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_divisor_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_remainder_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_target_read_and_resolve(state, scope, out, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }
    return 0;
}

uint64_t aml_quotient_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_target_read_and_resolve(state, scope, out, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }
    return 0;
}

static inline uint64_t aml_helper_op_operand_operand_target_read(aml_state_t* state, aml_scope_t* scope,
    aml_object_t** out, aml_token_num_t expectedOp, aml_data_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**, aml_object_t*, aml_object_t*))
{
    aml_token_t op;
    if (aml_token_read(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    if (op.num != expectedOp)
    {
        AML_DEBUG_ERROR(state, "Invalid %s '0x%x'", aml_token_lookup(expectedOp)->name, op.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, allowedTypes) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    // Operand2 must be the same type as operand1.
    aml_object_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, operand1->type) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (callback(state, scope, out, operand1, operand2) == ERR)
    {
        aml_object_deinit(*out);
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_convert_result(*out, target) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    return 0;
}

static inline uint64_t aml_def_add_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, operand1->integer.value + operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_add_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_ADD_OP, AML_DATA_INTEGER,
        aml_def_add_callback);
}

static inline uint64_t aml_def_subtract_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, operand1->integer.value - operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_subtract_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_SUBTRACT_OP, AML_DATA_INTEGER,
        aml_def_subtract_callback);
}

static inline uint64_t aml_def_multiply_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, operand1->integer.value * operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_multiply_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    (void)state;
    (void)scope;
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_MULTIPLY_OP, AML_DATA_INTEGER,
        aml_def_multiply_callback);
}

uint64_t aml_def_divide_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t divOp;
    if (aml_token_read_no_ext(state, &divOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DivideOp");
        return ERR;
    }

    if (divOp.num != AML_DIVIDE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid DivideOp '0x%x'", divOp.num);
        errno = EILSEQ;
        return ERR;
    }

    uint64_t result = ERR;

    uint64_t dividend;
    if (aml_dividend_read(state, scope, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Dividend");
        return ERR;
    }

    uint64_t divisor;
    if (aml_divisor_read(state, scope, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Divisor");
        return ERR;
    }

    if (divisor == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero");
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* remainderDest = NULL;
    if (aml_remainder_read(state, scope, &remainderDest) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read remainder");
        return ERR;
    }

    aml_object_t* quotientDest = NULL;
    if (aml_quotient_read(state, scope, &quotientDest) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read quotient");
        return ERR;
    }

    aml_object_t remainder = AML_OBJECT_CREATE(AML_OBJECT_NONE);
    if (aml_object_init_integer(&remainder, dividend % divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init remainder");
        return ERR;
    }

    aml_object_t quotient = AML_OBJECT_CREATE(AML_OBJECT_NONE);
    if (aml_object_init_integer(&quotient, dividend / divisor) == ERR)
    {
        aml_object_deinit(&remainder);
        AML_DEBUG_ERROR(state, "Failed to init quotient");
        return ERR;
    }

    if (quotientDest == NULL)
    {
        if (aml_convert_result(&quotient, quotientDest) == ERR)
        {
            aml_object_deinit(&remainder);
            aml_object_deinit(&quotient);
            return ERR;
        }
    }

    if (remainderDest != NULL)
    {
        if (aml_convert_result(&remainder, remainderDest) == ERR)
        {
            aml_object_deinit(&remainder);
            aml_object_deinit(&quotient);
            return ERR;
        }
    }

    aml_object_deinit(&remainder);

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        aml_object_deinit(&quotient);
        return ERR;
    }

    if (aml_object_init_integer(*out, quotient.integer.value) == ERR)
    {
        aml_object_deinit(&quotient);
        return ERR;
    }

    aml_object_deinit(&quotient);
    return 0;
}

uint64_t aml_def_mod_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t modOp;
    if (aml_token_read(state, &modOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ModOp");
        return ERR;
    }

    if (modOp.num != AML_MOD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ModOp '0x%x'", modOp.num);
        errno = EILSEQ;
        return ERR;
    }

    uint64_t dividend;
    if (aml_dividend_read(state, scope, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Dividend");
        return ERR;
    }

    uint64_t divisor;
    if (aml_divisor_read(state, scope, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Divisor");
        return ERR;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    if (divisor == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero");
        errno = EILSEQ;
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (aml_object_init_integer(*out, dividend % divisor) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_convert_result(*out, target) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    return 0;
}

static inline uint64_t aml_def_and_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, operand1->integer.value & operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_and_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_AND_OP, AML_DATA_INTEGER,
        aml_def_and_callback);
}

static inline uint64_t aml_def_nand_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, ~(operand1->integer.value & operand2->integer.value)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_nand_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_NAND_OP, AML_DATA_INTEGER,
        aml_def_nand_callback);
}

static inline uint64_t aml_def_or_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, operand1->integer.value | operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_or_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_OR_OP, AML_DATA_INTEGER,
        aml_def_or_callback);
}

static inline uint64_t aml_def_nor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, ~(operand1->integer.value | operand2->integer.value)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_nor_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_NOR_OP, AML_DATA_INTEGER,
        aml_def_nor_callback);
}

static inline uint64_t aml_def_xor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, operand1->integer.value ^ operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_xor_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_XOR_OP, AML_DATA_INTEGER,
        aml_def_xor_callback);
}

uint64_t aml_def_not_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t notOp;
    if (aml_token_read(state, &notOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NotOp");
        return ERR;
    }

    if (notOp.num != AML_NOT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid NotOp '0x%x'", notOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return ERR;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (aml_object_init_integer(*out, ~operand->integer.value) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_convert_result(*out, target) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_shift_count_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_shift_left_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t shlOp;
    if (aml_token_read(state, &shlOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftLeftOp");
        return ERR;
    }

    if (shlOp.num != AML_SHIFT_LEFT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ShiftLeftOp '0x%x'", shlOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return ERR;
    }

    uint64_t shiftCount;
    if (aml_shift_count_read(state, scope, &shiftCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftCount");
        return ERR;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    // C will discard the most significant bits
    if (shiftCount >= sizeof(uint64_t) * 8)
    {
        if (aml_object_init_integer(*out, 0) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_object_init_integer(*out, operand->integer.value << shiftCount) == ERR)
        {
            return ERR;
        }
    }

    if (target != NULL)
    {
        if (aml_convert_result(*out, target) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_def_shift_right_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t shrOp;
    if (aml_token_read(state, &shrOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftRightOp");
        return ERR;
    }

    if (shrOp.num != AML_SHIFT_RIGHT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid ShiftRightOp '0x%x'", shrOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return ERR;
    }

    uint64_t shiftCount;
    if (aml_shift_count_read(state, scope, &shiftCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftCount");
        return ERR;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    // C will zero the most significant bits
    if (shiftCount >= sizeof(uint64_t) * 8)
    {
        if (aml_object_init_integer(*out, 0) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_object_init_integer(*out, operand->integer.value >> shiftCount) == ERR)
        {
            return ERR;
        }
    }

    if (target != NULL)
    {
        if (aml_convert_result(*out, target) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    return 0;
}

/**
 * Helper that reads a structure like `Op SuperName`.
 */
static inline uint64_t aml_helper_op_supername_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_token_num_t expectedOp, aml_data_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**))
{
    aml_token_t op;
    if (aml_token_read(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    if (op.num != expectedOp)
    {
        AML_DEBUG_ERROR(state, "Invalid %s '0x%x'", aml_token_lookup(expectedOp)->name, op.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* superName;
    if (aml_super_name_read_and_resolve(state, scope, &superName, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (aml_convert_source(superName, *out, allowedTypes) == ERR)
    {
        return ERR;
    }

    if (callback(state, scope, out) == ERR)
    {
        aml_object_deinit(*out);
        return ERR;
    }

    if (aml_convert_result(*out, superName) == ERR)
    {
        aml_object_deinit(*out);
        return ERR;
    }

    return 0;
}

static uint64_t aml_increment_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    (void)state;
    (void)scope;
    (*out)->integer.value--;
    return 0;
}

uint64_t aml_def_increment_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_supername_read(state, scope, out, AML_INCREMENT_OP, AML_DATA_INTEGER, NULL);
}

static uint64_t aml_decrement_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    (void)state;
    (void)scope;
    (*out)->integer.value++;
    return 0;
}

uint64_t aml_def_decrement_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_supername_read(state, scope, out, AML_DECREMENT_OP, AML_DATA_INTEGER, NULL);
}

uint64_t aml_obj_reference_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_object_t* termArg = NULL;
    if (aml_term_arg_read(state, scope, &termArg, AML_DATA_OBJECT_REFERENCE | AML_DATA_STRING) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    if (termArg->type == AML_DATA_OBJECT_REFERENCE)
    {
        *out = termArg->objectReference.target;
        return 0;
    }
    else if (termArg->type == AML_DATA_STRING)
    {
        aml_object_t* target = aml_object_find(scope->location, termArg->string.content);
        if (target == NULL)
        {
            AML_DEBUG_ERROR(state, "Failed to find target scope '%s'", termArg->string.content);
            errno = EILSEQ;
            return ERR;
        }

        *out = target;
        return 0;
    }

    // Should never happen.
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_def_deref_of_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t derefOfOp;
    if (aml_token_read(state, &derefOfOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DerefOfOp");
        return ERR;
    }

    if (derefOfOp.num != AML_DEREF_OF_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid DerefOfOp '0x%x'", derefOfOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* obj = NULL;
    if (aml_obj_reference_read(state, scope, &obj) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ObjReference");
        return ERR;
    }

    if (obj == NULL)
    {
        AML_DEBUG_ERROR(state, "ObjReference is a null reference");
        errno = EILSEQ;
        return ERR;
    }

    *out = obj;
    return 0;
}

uint64_t aml_buff_pkg_str_obj_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_term_arg_read(state, scope, out, AML_DATA_BUFFER | AML_DATA_PACKAGE | AML_DATA_STRING) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_index_value_read(aml_state_t* state, aml_scope_t* scope, uint64_t* out)
{
    if (aml_term_arg_read_integer(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_index_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t indexOp;
    if (aml_token_read(state, &indexOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IndexOp");
        return ERR;
    }

    if (indexOp.num != AML_INDEX_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid IndexOp '0x%x'", indexOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* bufferPkgStrObj = NULL;
    if (aml_buff_pkg_str_obj_read(state, scope, &bufferPkgStrObj) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BuffPkgStrObj");
        return ERR;
    }

    uint64_t index;
    if (aml_index_value_read(state, scope, &index) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IndexValue");
        return ERR;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    switch (bufferPkgStrObj->type)
    {
    case AML_DATA_PACKAGE: // Section 19.6.63.1
    {
        if (index >= bufferPkgStrObj->package.length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for package");
            errno = EILSEQ;
            return ERR;
        }

        if (aml_object_init_object_reference(*out, bufferPkgStrObj->package.elements[index]) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for package element");
            return ERR;
        }
    }
    break;
    case AML_DATA_BUFFER: // Section 19.6.63.2
    {
        if (index >= bufferPkgStrObj->buffer.length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for buffer");
            errno = EILSEQ;
            return ERR;
        }

        if (aml_object_init_object_reference(*out, &bufferPkgStrObj->buffer.byteFields[index]) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for buffer");
            return ERR;
        }
    }
    break;
    case AML_DATA_STRING: // Section 19.6.63.3
    {
        if (index >= bufferPkgStrObj->string.length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for string");
            errno = EILSEQ;
            return ERR;
        }

        if (aml_object_init_object_reference(*out, &bufferPkgStrObj->string.byteFields[index]) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for string");
            return ERR;
        }
    }
    break;
    default:
        AML_DEBUG_ERROR(state, "Invalid type, expected buffer, package or string but got '%s'",
            aml_data_type_to_string(bufferPkgStrObj->type));
        errno = EILSEQ;
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_object_init_object_reference(target, (*out)->objectReference.target) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    return 0;
}

static inline uint64_t aml_helper_operand_operand_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_token_num_t expectedOp, aml_data_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**, aml_object_t*, aml_object_t*))
{
    aml_token_t op;
    if (aml_token_read(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    if (op.num != expectedOp)
    {
        AML_DEBUG_ERROR(state, "Invalid %s '0x%x'", aml_token_lookup(expectedOp)->name, op.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* operand1 = NULL;
    if (aml_operand_read(state, scope, &operand1, allowedTypes) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return ERR;
    }

    // Operand2 must be the same type as operand1.
    aml_object_t* operand2 = NULL;
    if (aml_operand_read(state, scope, &operand2, operand1->type) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (callback(state, scope, out, operand1, operand2) == ERR)
    {
        aml_object_deinit(*out);
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_helper_op_operand_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_token_num_t expectedOp, aml_data_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**, aml_object_t*))
{
    aml_token_t op;
    if (aml_token_read(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    if (op.num != expectedOp)
    {
        AML_DEBUG_ERROR(state, "Invalid %s '0x%x'", aml_token_lookup(expectedOp)->name, op.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, allowedTypes) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (callback(state, scope, out, operand) == ERR)
    {
        aml_object_deinit(*out);
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_def_land_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand1, operand2, AML_COMPARE_AND)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_land_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LAND_OP, AML_DATA_INTEGER, aml_def_land_callback);
}

static inline uint64_t aml_def_lequal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand1, operand2, AML_COMPARE_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lequal_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LEQUAL_OP,
        AML_DATA_INTEGER | AML_DATA_STRING | AML_DATA_BUFFER, aml_def_lequal_callback);
}

static inline uint64_t aml_def_lgreater_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand1, operand2, AML_COMPARE_GREATER)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lgreater_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LGREATER_OP,
        AML_DATA_INTEGER | AML_DATA_STRING | AML_DATA_BUFFER, aml_def_lgreater_callback);
}

static inline uint64_t aml_def_lgreater_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand1, operand2, AML_COMPARE_GREATER_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lgreater_equal_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LGREATER_EQUAL_OP,
        AML_DATA_INTEGER | AML_DATA_STRING | AML_DATA_BUFFER, aml_def_lgreater_equal_callback);
}

static inline uint64_t aml_def_lless_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand1, operand2, AML_COMPARE_LESS)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lless_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LLESS_OP,
        AML_DATA_INTEGER | AML_DATA_STRING | AML_DATA_BUFFER, aml_def_lless_callback);
}

static inline uint64_t aml_def_lless_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand1, operand2, AML_COMPARE_LESS_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lless_equal_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LLESS_EQUAL_OP,
        AML_DATA_INTEGER | AML_DATA_STRING | AML_DATA_BUFFER, aml_def_lless_equal_callback);
}

static inline uint64_t aml_def_lnot_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand, NULL, AML_COMPARE_NOT)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lnot_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_read(state, scope, out, AML_LNOT_OP,
        AML_DATA_INTEGER | AML_DATA_STRING | AML_DATA_BUFFER, aml_def_lnot_callback);
}

static inline uint64_t aml_def_lnot_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand1, operand2, AML_COMPARE_NOT_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lnot_equal_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LNOT_EQUAL_OP,
        AML_DATA_INTEGER | AML_DATA_STRING | AML_DATA_BUFFER, aml_def_lnot_equal_callback);
}

static inline uint64_t aml_def_lor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_object_init_integer(*out, aml_compare(operand1, operand2, AML_COMPARE_OR)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lor_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LOR_OP, AML_DATA_INTEGER, aml_def_lor_callback);
}

uint64_t aml_mutex_object_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_super_name_read_and_resolve(state, scope, out, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    if ((*out)->type != AML_DATA_MUTEX)
    {
        AML_DEBUG_ERROR(state, "Object is not a Mutex");
        errno = EILSEQ;
        return ERR;
    }

    return 0;
}

uint64_t aml_timeout_read(aml_state_t* state, uint16_t* out)
{
    if (aml_word_data_read(state, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read WordData");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_acquire_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t acquireOp;
    if (aml_token_read(state, &acquireOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read AcquireOp");
        return ERR;
    }

    if (acquireOp.num != AML_ACQUIRE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid AcquireOp '0x%x'", acquireOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* mutex = NULL;
    if (aml_mutex_object_read(state, scope, &mutex) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Mutex");
        return ERR;
    }

    assert(mutex->type == AML_DATA_MUTEX);

    uint16_t timeout;
    if (aml_timeout_read(state, &timeout) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Timeout");
        return ERR;
    }

    clock_t clockTimeout = (timeout == 0xFFFF) ? CLOCKS_NEVER : (clock_t)timeout * (CLOCKS_PER_SEC / 1000);
    uint64_t result = aml_mutex_stack_acquire(&state->mutexStack, mutex, clockTimeout);
    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to acquire mutex");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (aml_object_init_integer(*out, result) == ERR)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_expression_opcode_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_token_t op;
    if (aml_token_peek(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek op");
        return ERR;
    }

    if (op.props->type == AML_TOKEN_TYPE_NAME)
    {
        if (aml_method_invocation_read(state, scope, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read MethodInvocation");
            return ERR;
        }
        return 0;
    }

    uint64_t result = 0;
    switch (op.num)
    {
    case AML_BUFFER_OP:
    {
        *out = aml_scope_get_temp(scope);
        if (*out == NULL)
        {
            return ERR;
        }

        result = aml_def_buffer_read(state, scope, *out);
    }
    case AML_COND_REF_OF_OP:
        result = aml_def_cond_ref_of_read(state, scope, out);
        break;
    case AML_STORE_OP:
        result = aml_def_store_read(state, scope, out);
        break;
    case AML_ADD_OP:
        result = aml_def_add_read(state, scope, out);
        break;
    case AML_SUBTRACT_OP:
        result = aml_def_subtract_read(state, scope, out);
        break;
    case AML_MULTIPLY_OP:
        result = aml_def_multiply_read(state, scope, out);
        break;
    case AML_DIVIDE_OP:
        result = aml_def_divide_read(state, scope, out);
        break;
    case AML_MOD_OP:
        result = aml_def_mod_read(state, scope, out);
        break;
    case AML_AND_OP:
        result = aml_def_and_read(state, scope, out);
        break;
    case AML_NAND_OP:
        result = aml_def_nand_read(state, scope, out);
        break;
    case AML_OR_OP:
        result = aml_def_or_read(state, scope, out);
        break;
    case AML_NOR_OP:
        result = aml_def_nor_read(state, scope, out);
        break;
    case AML_XOR_OP:
        result = aml_def_xor_read(state, scope, out);
        break;
    case AML_NOT_OP:
        result = aml_def_not_read(state, scope, out);
        break;
    case AML_SHIFT_LEFT_OP:
        result = aml_def_shift_left_read(state, scope, out);
        break;
    case AML_SHIFT_RIGHT_OP:
        result = aml_def_shift_right_read(state, scope, out);
        break;
    case AML_INCREMENT_OP:
        result = aml_def_increment_read(state, scope, out);
        break;
    case AML_DECREMENT_OP:
        result = aml_def_decrement_read(state, scope, out);
        break;
    case AML_DEREF_OF_OP:
        result = aml_def_deref_of_read(state, scope, out);
        break;
    case AML_INDEX_OP:
        result = aml_def_index_read(state, scope, out);
        break;
    case AML_LAND_OP:
        result = aml_def_land_read(state, scope, out);
        break;
    case AML_LEQUAL_OP:
        result = aml_def_lequal_read(state, scope, out);
        break;
    case AML_LGREATER_OP:
        result = aml_def_lgreater_read(state, scope, out);
        break;
    case AML_LGREATER_EQUAL_OP:
        result = aml_def_lgreater_equal_read(state, scope, out);
        break;
    case AML_LLESS_OP:
        result = aml_def_lless_read(state, scope, out);
        break;
    case AML_LLESS_EQUAL_OP:
        result = aml_def_lless_equal_read(state, scope, out);
        break;
    case AML_LNOT_OP:
        result = aml_def_lnot_read(state, scope, out);
        break;
    case AML_LNOT_EQUAL_OP:
        result = aml_def_lnot_equal_read(state, scope, out);
        break;
    case AML_LOR_OP:
        result = aml_def_lor_read(state, scope, out);
        break;
    case AML_ACQUIRE_OP:
        result = aml_def_acquire_read(state, scope, out);
        break;
    default:
        AML_DEBUG_ERROR(state, "Unknown expression opcode '%s' (0x%04x)", op.props->name, op.num);
        errno = ENOSYS;
        return ERR;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read opcode '%s'", op.props->name);
        return ERR;
    }

    return 0;
}
