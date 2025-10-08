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
        out->args[i] = NULL;
        if (aml_term_arg_read(state, scope, &out->args[i], AML_ALL_TYPES) == ERR)
        {
            return ERR;
        }

        out->count++;
    }

    return 0;
}

uint64_t aml_method_invocation_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_object_t* target = NULL;
    if (aml_name_string_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve NameString");
        return ERR;
    }

    if (target->type == AML_METHOD)
    {
        aml_term_arg_list_t args = {0};
        if (aml_term_arg_list_read(state, scope, target->method.methodFlags.argCount, &args) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read method arguments");
            return ERR;
        }

        // Note that the arg objects are either temporary objects or objects in the namespace, we therefore dont deinit
        // them.

        *out = aml_scope_get_temp(scope);
        if (*out == NULL)
        {
            return ERR;
        }

        if (aml_method_evaluate(&target->method, args.count, args.args, *out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to evaluate method '%s' with %u arg(s)", AML_OBJECT_GET_NAME(target),
                args.count);
            return ERR;
        }

        return 0;
    }

    *out = target;
    return 0;
}

uint64_t aml_def_cond_ref_of_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_token_expect(state, AML_COND_REF_OF_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read CondRefOfOp");
        return ERR;
    }

    aml_object_t* source = NULL;
    if (aml_super_name_read_and_resolve(state, scope, &source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    aml_object_t* result = NULL;
    if (aml_target_read_and_resolve(state, scope, &result) == ERR)
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
        if (aml_integer_init(*out, 0) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init false integer");
            return ERR;
        }
        return 0;
    }

    if (result == NULL)
    {
        // Return true since source resolved to an object and result dident so we dont need to store anything.
        if (aml_integer_init(*out, 1) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init true integer");
            return ERR;
        }
        return 0;
    }

    // Store a reference to source in the result and return true.

    if (aml_object_reference_init(result, source) == ERR)
    {
        aml_object_deinit(*out);
        AML_DEBUG_ERROR(state, "Failed to init ObjectReference in result");
        return ERR;
    }

    if (aml_integer_init(*out, 1) == ERR)
    {
        aml_object_deinit(*out);
        AML_DEBUG_ERROR(state, "Failed to init true integer");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_store_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_token_expect(state, AML_STORE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read StoreOp");
        return ERR;
    }

    aml_object_t* source = NULL;
    if (aml_term_arg_read(state, scope, &source, AML_DATA_REF_OBJECTS) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    assert(source != NULL);

    aml_object_t* destination = NULL;
    if (aml_super_name_read_and_resolve(state, scope, &destination) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    assert(destination != NULL);

    if (aml_store(source, destination) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to store source '%s' in destination '%s'", AML_OBJECT_GET_NAME(source),
            AML_OBJECT_GET_NAME(destination));
        return ERR;
    }

    *out = destination;
    return 0;
}

uint64_t aml_operand_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out, aml_type_t allowedTypes)
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
    if (aml_target_read_and_resolve(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }
    return 0;
}

uint64_t aml_quotient_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_target_read_and_resolve(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }
    return 0;
}

static inline uint64_t aml_helper_op_operand_operand_target_read(aml_state_t* state, aml_scope_t* scope,
    aml_object_t** out, aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**, aml_object_t*, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
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
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
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
        if (aml_store(*out, target) == ERR)
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

    if (aml_integer_init(*out, operand1->integer.value + operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_add_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_ADD_OP, AML_INTEGER, aml_def_add_callback);
}

static inline uint64_t aml_def_subtract_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(*out, operand1->integer.value - operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_subtract_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_SUBTRACT_OP, AML_INTEGER,
        aml_def_subtract_callback);
}

static inline uint64_t aml_def_multiply_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(*out, operand1->integer.value * operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_multiply_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_MULTIPLY_OP, AML_INTEGER,
        aml_def_multiply_callback);
}

uint64_t aml_def_divide_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_token_expect(state, AML_DIVIDE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DivideOp");
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

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    // Init with remainder.
    if (aml_integer_init(*out, dividend % divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init remainder");
        return ERR;
    }

    if (remainderDest != NULL)
    {
        if (aml_store(*out, remainderDest) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    // Init with quotient.
    if (aml_integer_init(*out, dividend / divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init quotient");
        return ERR;
    }

    if (quotientDest != NULL)
    {
        if (aml_store(*out, quotientDest) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    // Qoutient stays in out.
    return 0;
}

uint64_t aml_def_mod_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_token_expect(state, AML_MOD_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ModOp");
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
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
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

    if (aml_integer_init(*out, dividend % divisor) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_store(*out, target) == ERR)
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

    if (aml_integer_init(*out, operand1->integer.value & operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_and_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_AND_OP, AML_INTEGER, aml_def_and_callback);
}

static inline uint64_t aml_def_nand_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(*out, ~(operand1->integer.value & operand2->integer.value)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_nand_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_NAND_OP, AML_INTEGER,
        aml_def_nand_callback);
}

static inline uint64_t aml_def_or_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(*out, operand1->integer.value | operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_or_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_OR_OP, AML_INTEGER, aml_def_or_callback);
}

static inline uint64_t aml_def_nor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(*out, ~(operand1->integer.value | operand2->integer.value)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_nor_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_NOR_OP, AML_INTEGER, aml_def_nor_callback);
}

static inline uint64_t aml_def_xor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;

    if (aml_integer_init(*out, operand1->integer.value ^ operand2->integer.value) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_xor_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_operand_target_read(state, scope, out, AML_XOR_OP, AML_INTEGER, aml_def_xor_callback);
}

uint64_t aml_def_not_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_token_expect(state, AML_NOT_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read NotOp");
        return ERR;
    }

    aml_object_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return ERR;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
        return ERR;
    }

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    uint64_t operandValue = operand->integer.value;
    if (aml_integer_init(*out, ~operandValue) == ERR)
    {
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_store(*out, target) == ERR)
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
    if (aml_token_expect(state, AML_SHIFT_LEFT_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftLeftOp");
        return ERR;
    }

    aml_object_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_INTEGER) == ERR)
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
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
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
        if (aml_integer_init(*out, 0) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        uint64_t operandValue = operand->integer.value;
        if (aml_integer_init(*out, operandValue << shiftCount) == ERR)
        {
            return ERR;
        }
    }

    if (target != NULL)
    {
        if (aml_store(*out, target) == ERR)
        {
            aml_object_deinit(*out);
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_def_shift_right_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_token_expect(state, AML_SHIFT_RIGHT_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ShiftRightOp");
        return ERR;
    }

    aml_object_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, AML_INTEGER) == ERR)
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
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
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
        if (aml_integer_init(*out, 0) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        uint64_t operandValue = operand->integer.value;
        if (aml_integer_init(*out, operandValue >> shiftCount) == ERR)
        {
            return ERR;
        }
    }

    if (target != NULL)
    {
        if (aml_store(*out, target) == ERR)
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
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    aml_object_t* superName;
    if (aml_super_name_read_and_resolve(state, scope, &superName) == ERR)
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
    (*out)->integer.value++;
    return 0;
}

uint64_t aml_def_increment_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_supername_read(state, scope, out, AML_INCREMENT_OP, AML_INTEGER, aml_increment_callback);
}

static uint64_t aml_decrement_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    (void)state;
    (void)scope;
    (*out)->integer.value--;
    return 0;
}

uint64_t aml_def_decrement_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_supername_read(state, scope, out, AML_DECREMENT_OP, AML_INTEGER, aml_decrement_callback);
}

uint64_t aml_obj_reference_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    aml_object_t* termArg = NULL;
    if (aml_term_arg_read(state, scope, &termArg, AML_OBJECT_REFERENCE | AML_STRING) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    if (termArg->type == AML_OBJECT_REFERENCE)
    {
        *out = termArg->objectReference.target;
        return 0;
    }
    else if (termArg->type == AML_STRING)
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
    if (aml_token_expect(state, AML_DEREF_OF_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read DerefOfOp");
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
    if (aml_term_arg_read(state, scope, out, AML_BUFFER | AML_PACKAGE | AML_STRING) == ERR)
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
    if (aml_token_expect(state, AML_INDEX_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read IndexOp");
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
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
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
    case AML_PACKAGE: // Section 19.6.63.1
    {
        aml_package_t* package = &bufferPkgStrObj->package;

        if (index >= package->length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for package (length %llu, index %llu)", package->length, index);
            errno = EILSEQ;
            return ERR;
        }

        if (aml_object_reference_init(*out, package->elements[index]) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for package element");
            return ERR;
        }
    }
    break;
    case AML_BUFFER: // Section 19.6.63.2
    {
        aml_buffer_t* buffer = &bufferPkgStrObj->buffer;
        if (index >= buffer->length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for buffer (length %llu, index %llu)", buffer->length, index);
            errno = EILSEQ;
            return ERR;
        }

        aml_object_t* byteField = aml_object_new(state, AML_OBJECT_NONE);
        if (byteField == NULL)
        {
            AML_DEBUG_ERROR(state, "Failed to allocate byteField");
            return ERR;
        }
        DEREF_DEFER(byteField);

        if (aml_buffer_field_init_buffer(byteField, buffer, index * 8, 8) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init byteField");
            return ERR;
        }

        if (aml_object_reference_init(*out, byteField) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for buffer byteField");
            return ERR;
        }
    }
    break;
    case AML_STRING: // Section 19.6.63.3
    {
        aml_string_t* string = &bufferPkgStrObj->string;
        if (index >= string->length)
        {
            AML_DEBUG_ERROR(state, "Index out of bounds for string (length %llu, index %llu)", string->length, index);
            errno = EILSEQ;
            return ERR;
        }

        aml_object_t* byteField = aml_object_new(state, AML_OBJECT_NONE);
        if (byteField == NULL)
        {
            AML_DEBUG_ERROR(state, "Failed to allocate byteField");
            return ERR;
        }
        DEREF_DEFER(byteField);

        if (aml_buffer_field_init_string(byteField, string, index * 8, 8) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init byteField");
            return ERR;
        }

        if (aml_object_reference_init(*out, byteField) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init ObjectReference for string byteField");
            return ERR;
        }

    }
    break;
    default:
        AML_DEBUG_ERROR(state, "Invalid type, expected buffer, package or string but got '%s'",
            aml_type_to_string(bufferPkgStrObj->type));
        errno = EILSEQ;
        return ERR;
    }

    if (target != NULL)
    {
        if (aml_store(*out, target) == ERR)
        {
            aml_object_deinit(*out);
            AML_DEBUG_ERROR(state, "Failed to init Target");
            return ERR;
        }
    }

    return 0;
}

static inline uint64_t aml_helper_operand_operand_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**, aml_object_t*, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
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
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
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
    if (aml_integer_init(*out, aml_compare(operand1, operand2, AML_COMPARE_AND)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_land_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LAND_OP, AML_INTEGER, aml_def_land_callback);
}

static inline uint64_t aml_def_lequal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(*out, aml_compare(operand1, operand2, AML_COMPARE_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lequal_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LEQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lequal_callback);
}

static inline uint64_t aml_def_lgreater_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(*out, aml_compare(operand1, operand2, AML_COMPARE_GREATER)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lgreater_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LGREATER_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lgreater_callback);
}

static inline uint64_t aml_def_lgreater_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(*out, aml_compare(operand1, operand2, AML_COMPARE_GREATER_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lgreater_equal_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LGREATER_EQUAL_OP,
        AML_INTEGER | AML_STRING | AML_BUFFER, aml_def_lgreater_equal_callback);
}

static inline uint64_t aml_def_lless_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(*out, aml_compare(operand1, operand2, AML_COMPARE_LESS)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lless_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LLESS_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lless_callback);
}

static inline uint64_t aml_def_lless_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(*out, aml_compare(operand1, operand2, AML_COMPARE_LESS_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lless_equal_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LLESS_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lless_equal_callback);
}

static inline uint64_t aml_def_lnot_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(*out, aml_compare(operand, NULL, AML_COMPARE_NOT)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lnot_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_read(state, scope, out, AML_LNOT_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lnot_callback);
}

static inline uint64_t aml_def_lnot_equal_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(*out, aml_compare(operand1, operand2, AML_COMPARE_NOT_EQUAL)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lnot_equal_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LNOT_EQUAL_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_lnot_equal_callback);
}

static inline uint64_t aml_def_lor_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand1, aml_object_t* operand2)
{
    (void)state;
    (void)scope;
    if (aml_integer_init(*out, aml_compare(operand1, operand2, AML_COMPARE_OR)) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_lor_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_operand_operand_read(state, scope, out, AML_LOR_OP, AML_INTEGER, aml_def_lor_callback);
}

uint64_t aml_mutex_object_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_super_name_read_and_resolve(state, scope, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve SuperName");
        return ERR;
    }

    if ((*out)->type != AML_MUTEX)
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
    if (aml_token_expect(state, AML_ACQUIRE_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read AcquireOp");
        return ERR;
    }

    aml_object_t* mutex = NULL;
    if (aml_mutex_object_read(state, scope, &mutex) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Mutex");
        return ERR;
    }

    assert(mutex->type == AML_MUTEX);

    uint16_t timeout;
    if (aml_timeout_read(state, &timeout) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Timeout");
        return ERR;
    }

    clock_t clockTimeout = (timeout == 0xFFFF) ? CLOCKS_NEVER : (clock_t)timeout * (CLOCKS_PER_SEC / 1000);
    // If timedout result == 1, else result == 0.
    uint64_t result = aml_mutex_acquire(&mutex->mutex.mutex, mutex->mutex.syncLevel, clockTimeout);
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

    if (aml_integer_init(*out, result) == ERR)
    {
        return ERR;
    }

    return 0;
}

/**
 * Helper that reads a structure like `Op Operand Target`.
 */
static inline uint64_t aml_helper_op_operand_target_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_token_num_t expectedOp, aml_type_t allowedTypes,
    uint64_t (*callback)(aml_state_t*, aml_scope_t*, aml_object_t**, aml_object_t*))
{
    if (aml_token_expect(state, expectedOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read %s", aml_token_lookup(expectedOp)->name);
        return ERR;
    }

    aml_object_t* operand = NULL;
    if (aml_operand_read(state, scope, &operand, allowedTypes) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read Operand");
        return ERR;
    }

    aml_object_t* target = NULL;
    if (aml_target_read_and_resolve(state, scope, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve Target");
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

static inline uint64_t aml_def_to_bcd_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
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

    if (aml_integer_init(*out, bcd) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_to_bcd_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_target_read(state, scope, out, AML_TO_BCD_OP, AML_INTEGER, aml_def_to_bcd_callback);
}

static inline uint64_t aml_def_to_buffer_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_convert_to_buffer(operand, *out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert to buffer");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_to_buffer_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_target_read(state, scope, out, AML_TO_BUFFER_OP, AML_INTEGER | AML_STRING | AML_BUFFER,
        aml_def_to_buffer_callback);
}

static inline uint64_t aml_def_to_decimal_string_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_convert_to_decimal_string(operand, *out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert to string");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_to_decimal_string_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_target_read(state, scope, out, AML_TO_DECIMAL_STRING_OP, AML_INTEGER,
        aml_def_to_decimal_string_callback);
}

static inline uint64_t aml_def_to_hex_string_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_convert_to_hex_string(operand, *out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert to string");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_to_hex_string_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_target_read(state, scope, out, AML_TO_HEX_STRING_OP, AML_INTEGER,
        aml_def_to_hex_string_callback);
}

static inline uint64_t aml_def_to_integer_callback(aml_state_t* state, aml_scope_t* scope, aml_object_t** out,
    aml_object_t* operand)
{
    (void)state;
    (void)scope;
    if (aml_convert_to_integer(operand, *out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert to integer");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_to_integer_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    return aml_helper_op_operand_target_read(state, scope, out, AML_TO_INTEGER_OP,
        AML_INTEGER | AML_STRING | AML_BUFFER, aml_def_to_integer_callback);
}

uint64_t aml_def_timer_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    if (aml_token_expect(state, AML_TIMER_OP) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TimerOp");
        return ERR;
    }

    // The period of the timer is supposed to be 100ns.
    uint64_t time100ns = (timer_uptime() * 10000000) / CLOCKS_PER_SEC;

    *out = aml_scope_get_temp(scope);
    if (*out == NULL)
    {
        return ERR;
    }

    if (aml_integer_init(*out, time100ns) == ERR)
    {
        aml_object_deinit(*out);
        return ERR;
    }

    return 0;
}

uint64_t aml_expression_opcode_read(aml_state_t* state, aml_scope_t* scope, aml_object_t** out)
{
    assert(out != NULL);

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
    break;
    case AML_PACKAGE_OP:
    {
        *out = aml_scope_get_temp(scope);
        if (*out == NULL)
        {
            return ERR;
        }

        result = aml_def_package_read(state, scope, *out);
    }
    break;
    case AML_VAR_PACKAGE_OP:
    {
        *out = aml_scope_get_temp(scope);
        if (*out == NULL)
        {
            return ERR;
        }

        result = aml_def_var_package_read(state, scope, *out);
    }
    break;
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
    case AML_TO_BCD_OP:
        result = aml_def_to_bcd_read(state, scope, out);
        break;
    case AML_TO_BUFFER_OP:
        result = aml_def_to_buffer_read(state, scope, out);
        break;
    case AML_TO_DECIMAL_STRING_OP:
        result = aml_def_to_decimal_string_read(state, scope, out);
        break;
    case AML_TO_HEX_STRING_OP:
        result = aml_def_to_hex_string_read(state, scope, out);
        break;
    case AML_TO_INTEGER_OP:
        result = aml_def_to_integer_read(state, scope, out);
        break;
    case AML_TIMER_OP:
        result = aml_def_timer_read(state, scope, out);
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
