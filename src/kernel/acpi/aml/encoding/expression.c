#include "expression.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_object.h"
#include "acpi/aml/aml_scope.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_token.h"
#include "acpi/aml/runtime/compare.h"
#include "acpi/aml/runtime/convert.h"
#include "acpi/aml/runtime/copy.h"
#include "acpi/aml/runtime/method.h"
#include "acpi/aml/runtime/store.h"
#include "arg.h"
#include "mem/heap.h"
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
    if (aml_token_expect(state, AML_BUFFER_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read BufferOp");
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

    if (aml_buffer_init(out, state->current, availableBytes, bufferSize) == ERR)
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
        out->args[i] = aml_term_arg_read(state, scope, AML_DATA_REF_OBJECTS);
        if (out->args[i] == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                DEREF(out->args[j]);
                out->args[j] = NULL;
            }
            return ERR;
        }

        out->count++;
    }

    return 0;
}

aml_object_t* aml_method_invocation_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_object_t* target = aml_name_string_read_and_resolve(state, scope);
    if (target == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve NameString");
        return NULL;
    }
    DEREF_DEFER(target);

    if (target->type == AML_METHOD)
    {
        aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
        if (result == NULL)
        {
            return NULL;
        }
        DEREF_DEFER(result);

        aml_term_arg_list_t args = {0};
        if (aml_term_arg_list_read(state, scope, target->method.methodFlags.argCount, &args) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read method arguments");
            return NULL;
        }

        if (aml_method_evaluate(&target->method, args.count, args.args, result) == ERR)
        {
            for (uint8_t i = 0; i < args.count; i++)
            {
                DEREF(args.args[i]);
                args.args[i] = NULL;
            }
            AML_DEBUG_ERROR(state, "Failed to evaluate method '%s' with %u arg(s)", AML_OBJECT_GET_NAME(target),
                args.count);
            return NULL;
        }

        for (uint8_t i = 0; i < args.count; i++)
        {
            DEREF(args.args[i]);
            args.args[i] = NULL;
        }

        return REF(result);
    }

    return REF(target);
}

aml_object_t* aml_def_cond_ref_of_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_COND_REF_OF_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read CondRefOfOp");
        return NULL;
    }

    aml_object_t* source = aml_super_name_read_and_resolve(state, scope);
    if (source == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return NULL;
    }
    DEREF_DEFER(source);

    aml_object_t* result = NULL;
    if (aml_target_read_and_resolve(state, scope, &result) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(result);

    aml_object_t* output = aml_object_new(state, AML_OBJECT_NONE);
    if (output == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(output);

    if (source == NULL)
    {
        // Return false since the source did not resolve to an object.
        if (aml_integer_init(output, 0) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init false integer");
            return NULL;
        }
        return REF(output);
    }

    if (result == NULL)
    {
        // Return true since source resolved to an object and result dident so we dont need to store anything.
        if (aml_integer_init(output, 1) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init true integer");
            return NULL;
        }
        return REF(output);
    }

    // Store a reference to source in the result and return true.

    if (aml_object_reference_init(result, source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init ObjectReference in result");
        return NULL;
    }

    if (aml_integer_init(output, 1) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init true integer");
        return NULL;
    }

    return REF(output);
}

aml_object_t* aml_def_store_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_STORE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read StoreOp");
        return NULL;
    }

    aml_object_t* source = aml_term_arg_read(state, scope, AML_DATA_REF_OBJECTS);
    if (source == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return NULL;
    }
    DEREF_DEFER(source);

    aml_object_t* destination = aml_super_name_read_and_resolve(state, scope);
    if (destination == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return NULL;
    }
    DEREF_DEFER(destination);

    if (aml_store(source, destination) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to store source '%s' in destination '%s'", AML_OBJECT_GET_NAME(source),
            AML_OBJECT_GET_NAME(destination));
        return NULL;
    }

    return REF(source);
}

aml_object_t* aml_operand_read(aml_state_t* state, aml_scope_t* scope, aml_type_t allowedTypes)
{
    aml_object_t* result = aml_term_arg_read(state, scope, allowedTypes);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return NULL;
    }

    return result; // Transfer ownership
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

aml_object_t* aml_remainder_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_object_t* result = NULL;
    if (aml_target_read_and_resolve(state, scope, &result) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }

    return result; // Transfer ownership
}

aml_object_t* aml_quotient_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_object_t* result = NULL;
    if (aml_target_read_and_resolve(state, scope, &result) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }

    return result; // Transfer ownership
}

static inline aml_object_t* aml_helper_op_operand_operand_target_read(aml_state_t* state, aml_scope_t* scope,
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t*, aml_object_t*, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return NULL;
    }

    aml_object_t* operand1 = aml_operand_read(state, scope, allowedTypes);
    if (operand1 == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return NULL;
    }
    DEREF_DEFER(operand1);

    // Operand2 must be the same type as operand1.
    aml_object_t* operand2 = aml_operand_read(state, scope, operand1->type);
    if (operand2 == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return NULL;
    }
    DEREF_DEFER(operand2);

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (callback(state, scope, result, operand1, operand2) == ERR)
    {
        return NULL;
    }

    if (target != NULL)
    {
        if (aml_store(result, target) == ERR)
        {
            return NULL;
        }
    }

    return REF(result);
}

static inline uint64_t aml_def_add_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(out, operand1->integer.value + operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_add_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_operand_target_read(state, scope, AML_ADD_OP, AML_INTEGER, aml_def_add_callback);
}

static inline uint64_t aml_def_subtract_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(out, operand1->integer.value - operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_subtract_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_operand_target_read(state, scope, AML_SUBTRACT_OP, AML_INTEGER,
        aml_def_subtract_callback);
}

static inline uint64_t aml_def_multiply_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(out, operand1->integer.value * operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_multiply_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_operand_target_read(state, scope, AML_MULTIPLY_OP, AML_INTEGER,
        aml_def_multiply_callback);
}

aml_object_t* aml_def_divide_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_DIVIDE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DivideOp");
        return NULL;
    }

    uint64_t dividend;
    if (aml_dividend_read(state, scope, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Dividend");
        return NULL;
    }

    uint64_t divisor;
    if (aml_divisor_read(state, scope, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Divisor");
        return NULL;
    }

    if (divisor == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero");
        errno = EILSEQ;
        return NULL;
    }

    aml_object_t* remainderDest = aml_remainder_read(state, scope);
    if (remainderDest == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read remainder");
        return NULL;
    }
    DEREF_DEFER(remainderDest);

    aml_object_t* quotientDest = aml_quotient_read(state, scope);
    if (quotientDest == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read quotient");
        return NULL;
    }
    DEREF_DEFER(quotientDest);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    // Init with remainder.
    if (aml_integer_init(result, dividend % divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init remainder");
        return NULL;
    }

    if (remainderDest != NULL)
    {
        if (aml_store(result, remainderDest) == ERR)
        {
            return NULL;
        }
    }

    // Init with quotient.
    if (aml_integer_init(result, dividend / divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init quotient");
        return NULL;
    }

    if (quotientDest != NULL)
    {
        if (aml_store(result, quotientDest) == ERR)
        {
            return NULL;
        }
    }

    // Qoutient stays in result.
    return REF(result);
}

aml_object_t* aml_def_mod_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_MOD_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ModOp");
        return NULL;
    }

    uint64_t dividend;
    if (aml_dividend_read(state, scope, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Dividend");
        return NULL;
    }

    uint64_t divisor;
    if (aml_divisor_read(state, scope, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Divisor");
        return NULL;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    if (divisor == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero");
        errno = EILSEQ;
        return NULL;
    }

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_init(result, dividend % divisor) == ERR)
    {
        return NULL;
    }

    if (target != NULL)
    {
        if (aml_store(result, target) == ERR)
        {
            return NULL;
        }
    }

    return REF(result);
}

static inline uint64_t aml_def_and_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(out, operand1->integer.value & operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_and_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_operand_target_read(state, scope, AML_AND_OP, AML_INTEGER, aml_def_and_callback);
}

static inline uint64_t aml_def_nand_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(out, ~(operand1->integer.value & operand2->integer.value)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_nand_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_operand_target_read(state, scope, AML_NAND_OP, AML_INTEGER, aml_def_nand_callback);
}

static inline uint64_t aml_def_or_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(out, operand1->integer.value | operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_or_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_operand_target_read(state, scope, AML_OR_OP, AML_INTEGER, aml_def_or_callback);
}

static inline uint64_t aml_def_nor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(out, ~(operand1->integer.value | operand2->integer.value)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_nor_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_operand_target_read(state, scope, AML_NOR_OP, AML_INTEGER, aml_def_nor_callback);
}

static inline uint64_t aml_def_xor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(out, operand1->integer.value ^ operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_xor_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_operand_target_read(state, scope, AML_XOR_OP, AML_INTEGER, aml_def_xor_callback);
}

aml_object_t* aml_def_not_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_NOT_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NotOp");
        return NULL;
    }

    aml_object_t* operand = aml_operand_read(state, scope, AML_INTEGER);
    if (operand == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return NULL;
    }
    DEREF_DEFER(operand);

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    uint64_t operandValue = operand->integer.value;
    if (aml_integer_init(result, ~operandValue) == ERR)
    {
        return NULL;
    }

    if (target != NULL)
    {
        if (aml_store(result, target) == ERR)
        {
            return NULL;
        }
    }

    return REF(result);
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

aml_object_t* aml_def_shift_left_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_SHIFT_LEFT_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftLeftOp");
        return NULL;
    }

    aml_object_t* operand = aml_operand_read(state, scope, AML_INTEGER);
    if (operand == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return NULL;
    }
    DEREF_DEFER(operand);

    uint64_t shiftCount;
    if (aml_shift_count_read(state, scope, &shiftCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftCount");
        return NULL;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    // C will discard the most significant bits
    if (shiftCount >= sizeof(uint64_t) * 8)
    {
        if (aml_integer_init(result, 0) == ERR)
        {
            return NULL;
        }
    }
    else
    {
        uint64_t operandValue = operand->integer.value;
        if (aml_integer_init(result, operandValue << shiftCount) == ERR)
        {
            return NULL;
        }
    }

    if (target != NULL)
    {
        if (aml_store(result, target) == ERR)
        {
            return NULL;
        }
    }

    return REF(result);
}

aml_object_t* aml_def_shift_right_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_SHIFT_RIGHT_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftRightOp");
        return NULL;
    }

    aml_object_t* operand = aml_operand_read(state, scope, AML_INTEGER);
    if (operand == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return NULL;
    }
    DEREF_DEFER(operand);

    uint64_t shiftCount;
    if (aml_shift_count_read(state, scope, &shiftCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftCount");
        return NULL;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    // C will zero the most significant bits
    if (shiftCount >= sizeof(uint64_t) * 8)
    {
        if (aml_integer_init(result, 0) == ERR)
        {
            return NULL;
        }
    }
    else
    {
        uint64_t operandValue = operand->integer.value;
        if (aml_integer_init(result, operandValue >> shiftCount) == ERR)
        {
            return NULL;
        }
    }

    if (target != NULL)
    {
        if (aml_store(result, target) == ERR)
        {
            return NULL;
        }
    }

    return REF(result);
}

/**
 * Helper that reads a structure like `Op SuperName`.
 */
static inline aml_object_t* aml_helper_op_supername_read(aml_state_t* state, aml_scope_t* scope,
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return NULL;
    }

    aml_object_t* superName = aml_super_name_read_and_resolve(state, scope);
    if (superName == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return NULL;
    }
    DEREF_DEFER(superName);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_convert_source(superName, result, allowedTypes) == ERR)
    {
        return NULL;
    }

    if (callback(state, scope, result) == ERR)
    {
        return NULL;
    }

    if (aml_convert_result(result, superName) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

static uint64_t aml_increment_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    (void)state;
    (void)scope;
    (out)->integer.value++;
    return 0;
}

aml_object_t* aml_def_increment_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_supername_read(state, scope, AML_INCREMENT_OP, AML_INTEGER, aml_increment_callback);
}

static uint64_t aml_decrement_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out)
{
    (void)state;
    (void)scope;
    (out)->integer.value--;
    return 0;
}

aml_object_t* aml_def_decrement_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_supername_read(state, scope, AML_DECREMENT_OP, AML_INTEGER, aml_decrement_callback);
}

aml_object_t* aml_obj_reference_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_object_t* termArg = aml_term_arg_read(state, scope, AML_OBJECT_REFERENCE | AML_STRING);
    if (termArg == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return NULL;
    }
    DEREF_DEFER(termArg);

    if (termArg->type == AML_OBJECT_REFERENCE)
    {
        return REF(termArg->objectReference.target);
    }
    else if (termArg->type == AML_STRING)
    {
        aml_object_t* target = aml_object_find(scope->location, termArg->string.content);
        if (target == NULL)
        {
            AML_DEBUG_ERROR(state, "Failed to find target scope '%s'", termArg->string.content);
            errno = EILSEQ;
            return NULL;
        }

        return target; // Transfer ownership
    }

    // Should never happen.
    errno = EILSEQ;
    return NULL;
}

aml_object_t* aml_def_deref_of_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_DEREF_OF_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DerefOfOp");
        return NULL;
    }

    aml_object_t* obj = aml_obj_reference_read(state, scope);
    if (obj == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read ObjReference");
        return NULL;
    }

    return obj; // Transfer ownership
}

aml_object_t* aml_buff_pkg_str_obj_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_object_t* result = aml_term_arg_read(state, scope, AML_BUFFER | AML_PACKAGE | AML_STRING);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return NULL;
    }

    return result; // Transfer ownership
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

aml_object_t* aml_def_index_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_INDEX_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IndexOp");
        return NULL;
    }

    aml_object_t* buffPkgStrObj = aml_buff_pkg_str_obj_read(state, scope);
    if (buffPkgStrObj == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read BuffPkgStrObj");
        return NULL;
    }
    DEREF_DEFER(buffPkgStrObj);

    uint64_t index;
    if (aml_index_value_read(state, scope, &index) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IndexValue");
        return NULL;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
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
            AML_DEBUG_ERROR(state, "Index out of bounds for package (length %llu, index %llu)", package->length, index);
            errno = EILSEQ;
            return NULL;
        }

        if (aml_object_reference_init(result, package->elements[index]) == ERR)
        {
            return NULL;
        }
    }
    break;
    case AML_BUFFER: // Section 19.6.63.2
    {
        aml_buffer_t* buffer = &buffPkgStrObj->buffer;
        if (index >= buffer->length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for buffer (length %llu, index %llu)", buffer->length, index);
            errno = EILSEQ;
            return NULL;
        }

        aml_object_t* byteField = aml_object_new(state, AML_OBJECT_NONE);
        if (byteField == NULL)
        {
            return NULL;
        }
        DEREF_DEFER(byteField);

        if (aml_buffer_field_init_buffer(byteField, buffer, index * 8, 8) == ERR)
        {
            return NULL;
        }

        if (aml_object_reference_init(result, byteField) == ERR)
        {
            return NULL;
        }
    }
    break;
    case AML_STRING: // Section 19.6.63.3
    {
        aml_string_t* string = &buffPkgStrObj->string;
        if (index >= string->length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for string (length %llu, index %llu)", string->length, index);
            errno = EILSEQ;
            return NULL;
        }

        aml_object_t* byteField = aml_object_new(state, AML_OBJECT_NONE);
        if (byteField == NULL)
        {
            return NULL;
        }
        DEREF_DEFER(byteField);

        if (aml_buffer_field_init_string(byteField, string, index * 8, 8) == ERR)
        {
            return NULL;
        }

        if (aml_object_reference_init(result, byteField) == ERR)
        {
            return NULL;
        }
    }
    break;
    default:
        AML_DEBUG_ERROR(state, "Invalid type, expected buffer, package or string but got '%s'",
            aml_type_to_string(buffPkgStrObj->type));
        errno = EILSEQ;
        return NULL;
    }

    if (target != NULL)
    {
        if (aml_store(result, target) == ERR)
        {
            return NULL;
        }
    }

    return REF(result);
}

static inline aml_object_t* aml_helper_operand_operand_read(aml_state_t* state, aml_scope_t* scope,
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t*, aml_object_t*, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return NULL;
    }

    aml_object_t* operand1 = aml_operand_read(state, scope, allowedTypes);
    if (operand1 == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand1");
        return NULL;
    }
    DEREF_DEFER(operand1);

    // Operand2 must be the same type as operand1.
    aml_object_t* operand2 = aml_operand_read(state, scope, operand1->type);
    if (operand2 == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand2");
        return NULL;
    }
    DEREF_DEFER(operand2);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (callback(state, scope, result, operand1, operand2) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

static inline aml_object_t* aml_helper_op_operand_read(aml_state_t* state, aml_scope_t* scope,
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t*, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return NULL;
    }

    aml_object_t* operand = aml_operand_read(state, scope, allowedTypes);
    if (operand == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand");
        return NULL;
    }
    DEREF_DEFER(operand);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (callback(state, scope, result, operand) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

static inline uint64_t aml_def_land_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand1, operand2, AML_COMPARE_AND)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_land_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_operand_operand_read(state, scope, AML_LAND_OP, AML_INTEGER, aml_def_land_callback);
}

static inline uint64_t aml_def_lequal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand1, operand2, AML_COMPARE_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_lequal_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_operand_operand_read(state, scope, AML_LEQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lequal_callback);
}

static inline uint64_t aml_def_lgreater_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand1, operand2, AML_COMPARE_GREATER)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_lgreater_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_operand_operand_read(state, scope, AML_LGREATER_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lgreater_callback);
}

static inline uint64_t aml_def_lgreater_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand1, operand2, AML_COMPARE_GREATER_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_lgreater_equal_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_operand_operand_read(state, scope, AML_LGREATER_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lgreater_equal_callback);
}

static inline uint64_t aml_def_lless_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand1, operand2, AML_COMPARE_LESS)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_lless_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_operand_operand_read(state, scope, AML_LLESS_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lless_callback);
}

static inline uint64_t aml_def_lless_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand1, operand2, AML_COMPARE_LESS_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_lless_equal_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_operand_operand_read(state, scope, AML_LLESS_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lless_equal_callback);
}

static inline uint64_t aml_def_lnot_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand, NULL, AML_COMPARE_NOT)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_lnot_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_read(state, scope, AML_LNOT_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lnot_callback);
}

static inline uint64_t aml_def_lnot_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand1, operand2, AML_COMPARE_NOT_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_lnot_equal_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_operand_operand_read(state, scope, AML_LNOT_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lnot_equal_callback);
}

static inline uint64_t aml_def_lor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(out, aml_compare(operand1, operand2, AML_COMPARE_OR)) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_lor_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_operand_operand_read(state, scope, AML_LOR_OP, AML_INTEGER, aml_def_lor_callback);
}

aml_object_t* aml_mutex_object_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_object_t* result = aml_super_name_read_and_resolve(state, scope);
    if (result == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return NULL;
    }
    DEREF_DEFER(result);

    if (result->type != AML_MUTEX)
    {
        AML_DEBUG_ERROR(state, "Object is not a Mutex");
        errno = EILSEQ;
        return NULL;
    }

    return REF(result);
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

aml_object_t* aml_def_acquire_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_ACQUIRE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read AcquireOp");
        return NULL;
    }

    aml_object_t* mutex = aml_mutex_object_read(state, scope);
    if (mutex == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read Mutex");
        return NULL;
    }
    DEREF_DEFER(mutex);

    assert(mutex->type == AML_MUTEX);

    uint16_t timeout;
    if (aml_timeout_read(state, &timeout) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Timeout");
        return NULL;
    }

    clock_t clockTimeout = (timeout == 0xFFFF) ? CLOCKS_NEVER : (clock_t)timeout * (CLOCKS_PER_SEC / 1000);
    // If timedout result == 1, else result == 0.
    uint64_t acquireResult = aml_mutex_acquire(&mutex->mutex.mutex, mutex->mutex.syncLevel, clockTimeout);
    if (acquireResult == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to acquire mutex");
        return NULL;
    }

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (aml_integer_init(result, acquireResult) == ERR)
    {
        return NULL;
    }

    return REF(result);
}

/**
 * Helper that reads a structure like `Op Operand Target`.
 */
static inline aml_object_t* aml_helper_op_operand_target_read(aml_state_t* state, aml_scope_t* scope,
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t*, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return NULL;
    }

    aml_object_t* operand = aml_operand_read(state, scope, allowedTypes);
    if (operand == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return NULL;
    }
    DEREF_DEFER(operand);

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return NULL;
    }
    DEREF_DEFER(target);

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(result);

    if (callback(state, scope, result, operand) == ERR)
    {
        return NULL;
    }

    if (target != NULL)
    {
        if (aml_convert_result(result, target) == ERR)
        {
            return NULL;
        }
    }

    return REF(result);
}

static inline uint64_t aml_def_to_bcd_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;

    uint64_t bcd;
    if (aml_convert_integer_to_bcd(operand->integer.value, &bcd) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert integer to BCD");
        return ERR;
    }

    if (aml_integer_init(out, bcd) == ERR)
    {
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_to_bcd_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_target_read(state, scope, AML_TO_BCD_OP, AML_INTEGER, aml_def_to_bcd_callback);
}

static inline uint64_t aml_def_to_buffer_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_convert_to_buffer(operand, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert to buffer");
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_to_buffer_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_target_read(state, scope, AML_TO_BUFFER_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_to_buffer_callback);
}

static inline uint64_t aml_def_to_decimal_string_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_convert_to_decimal_string(operand, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert to string");
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_to_decimal_string_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_target_read(state, scope, AML_TO_DECIMAL_STRING_OP, AML_INTEGER,
        aml_def_to_decimal_string_callback);
}

static inline uint64_t aml_def_to_hex_string_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_convert_to_hex_string(operand, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert to string");
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_to_hex_string_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_target_read(state, scope, AML_TO_HEX_STRING_OP, AML_INTEGER,
        aml_def_to_hex_string_callback);
}

static inline uint64_t aml_def_to_integer_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t* out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_convert_to_integer(operand, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert to integer");
        return ERR;
    }
    return 0;
}

aml_object_t* aml_def_to_integer_read(aml_state_t* state, aml_scope_t* scope)
{
    return aml_helper_op_operand_target_read(state, scope, AML_TO_INTEGER_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_to_integer_callback);
}

aml_object_t* aml_def_timer_read(aml_state_t* state)
{
    if (aml_token_expect(state, AML_TIMER_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TimerOp");
        return NULL;
    }

    // The period of the timer is supposed to be 100ns.
    uint64_t time100ns = (timer_uptime() * 10000000) / CLOCKS_PER_SEC;

    aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
    if (result == NULL)
    {
        return NULL;
    }

    if (aml_integer_init(result, time100ns) == ERR)
    {
        aml_object_deinit(result);
        return NULL;
    }

    return result;
}

aml_object_t* aml_def_copy_object_read(aml_state_t* state, aml_scope_t* scope)
{
    if (aml_token_expect(state, AML_COPY_OBJECT_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read CopyObjectOp");
        return NULL;
    }

    aml_object_t* source = aml_term_arg_read(state, scope, AML_DATA_REF_OBJECTS);
    if (source == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read Source");
        return NULL;
    }
    DEREF_DEFER(source);

    aml_object_t* destination = aml_simple_name_read_and_resolve(state, scope);
    if (destination == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Destination");
        return NULL;
    }
    DEREF_DEFER(destination);

    if (aml_copy_object(source, destination) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to copy object");
        return NULL;
    }

    return REF(source);
}

aml_object_t* aml_expression_opcode_read(aml_state_t* state, aml_scope_t* scope)
{
    aml_token_t op;
    if (aml_token_peek(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek op");
        return NULL;
    }

    if (op.props->type == AML_TOKEN_TYPE_NAME)
    {
        aml_object_t* result = aml_method_invocation_read(state, scope);
        if (result == NULL)
        {
            AML_DEBUG_ERROR(state, "Failed to read MethodInvocation");
            return NULL;
        }
        return result;
    }

    aml_object_t* result = NULL;
    switch (op.num)
    {
    case AML_BUFFER_OP:
    {
        result = aml_object_new(state, AML_OBJECT_NONE);
        if (result == NULL)
        {
            return NULL;
        }

        if (aml_def_buffer_read(state, scope, result) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read opcode 'DefBuffer'");
            return NULL;
        }
    }
    break;
    case AML_PACKAGE_OP:
    {
        aml_object_t* result = aml_object_new(state, AML_OBJECT_NONE);
        if (result == NULL)
        {
            return NULL;
        }

        if (aml_def_package_read(state, scope, result) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read opcode 'DefPackage'");
            return NULL;
        }
    }
    break;
    case AML_VAR_PACKAGE_OP:
    {
        result = aml_object_new(state, AML_OBJECT_NONE);
        if (result == NULL)
        {
            return NULL;
        }

        if (aml_def_var_package_read(state, scope, result) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read opcode 'DefVarPackage'");
            return NULL;
        }
    }
    break;
    case AML_COND_REF_OF_OP:
        result = aml_def_cond_ref_of_read(state, scope);
        break;
    case AML_STORE_OP:
        result = aml_def_store_read(state, scope);
        break;
    case AML_ADD_OP:
        result = aml_def_add_read(state, scope);
        break;
    case AML_SUBTRACT_OP:
        result = aml_def_subtract_read(state, scope);
        break;
    case AML_MULTIPLY_OP:
        result = aml_def_multiply_read(state, scope);
        break;
    case AML_DIVIDE_OP:
        result = aml_def_divide_read(state, scope);
        break;
    case AML_MOD_OP:
        result = aml_def_mod_read(state, scope);
        break;
    case AML_AND_OP:
        result = aml_def_and_read(state, scope);
        break;
    case AML_NAND_OP:
        result = aml_def_nand_read(state, scope);
        break;
    case AML_OR_OP:
        result = aml_def_or_read(state, scope);
        break;
    case AML_NOR_OP:
        result = aml_def_nor_read(state, scope);
        break;
    case AML_XOR_OP:
        result = aml_def_xor_read(state, scope);
        break;
    case AML_NOT_OP:
        result = aml_def_not_read(state, scope);
        break;
    case AML_SHIFT_LEFT_OP:
        result = aml_def_shift_left_read(state, scope);
        break;
    case AML_SHIFT_RIGHT_OP:
        result = aml_def_shift_right_read(state, scope);
        break;
    case AML_INCREMENT_OP:
        result = aml_def_increment_read(state, scope);
        break;
    case AML_DECREMENT_OP:
        result = aml_def_decrement_read(state, scope);
        break;
    case AML_DEREF_OF_OP:
        result = aml_def_deref_of_read(state, scope);
        break;
    case AML_INDEX_OP:
        result = aml_def_index_read(state, scope);
        break;
    case AML_LAND_OP:
        result = aml_def_land_read(state, scope);
        break;
    case AML_LEQUAL_OP:
        result = aml_def_lequal_read(state, scope);
        break;
    case AML_LGREATER_OP:
        result = aml_def_lgreater_read(state, scope);
        break;
    case AML_LGREATER_EQUAL_OP:
        result = aml_def_lgreater_equal_read(state, scope);
        break;
    case AML_LLESS_OP:
        result = aml_def_lless_read(state, scope);
        break;
    case AML_LLESS_EQUAL_OP:
        result = aml_def_lless_equal_read(state, scope);
        break;
    case AML_LNOT_OP:
        result = aml_def_lnot_read(state, scope);
        break;
    case AML_LNOT_EQUAL_OP:
        result = aml_def_lnot_equal_read(state, scope);
        break;
    case AML_LOR_OP:
        result = aml_def_lor_read(state, scope);
        break;
    case AML_ACQUIRE_OP:
        result = aml_def_acquire_read(state, scope);
        break;
    case AML_TO_BCD_OP:
        result = aml_def_to_bcd_read(state, scope);
        break;
    case AML_TO_BUFFER_OP:
        result = aml_def_to_buffer_read(state, scope);
        break;
    case AML_TO_DECIMAL_STRING_OP:
        result = aml_def_to_decimal_string_read(state, scope);
        break;
    case AML_TO_HEX_STRING_OP:
        result = aml_def_to_hex_string_read(state, scope);
        break;
    case AML_TO_INTEGER_OP:
        result = aml_def_to_integer_read(state, scope);
        break;
    case AML_TIMER_OP:
        result = aml_def_timer_read(state);
        break;
    case AML_COPY_OBJECT_OP:
        result = aml_def_copy_object_read(state, scope);
        break;
    default:
        AML_DEBUG_ERROR(state, "Unknown ExpressionOpcode '%s' (0x%04x)", op.props->name, op.num);
        errno = ENOSYS;
        return NULL;
    }

    if (result == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to read ExpressionOpcode '%s' (0x%04x)", op.props->name, op.num);
        return NULL;
    }

    return result; // Transfer ownership
}
