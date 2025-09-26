#include "expression.h"

#include "acpi/aml/aml_convert.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "acpi/aml/runtime/method.h"
#include "arg.h"
#include "package_length.h"
#include "term.h"

typedef uint64_t (*aml_unary_op_t)(uint64_t);
typedef uint64_t (*aml_binary_op_t)(uint64_t, uint64_t);

/**
 * Helper for reading and executing a structure in the format `OpCode Operand Target`.
 */
static inline uint64_t aml_unary_op_read(aml_state_t* state, aml_node_t* node, aml_node_t* out, aml_value_num_t opCode,
    const char* opName, aml_unary_op_t op)
{
    aml_value_t opValue;
    if (aml_value_read(state, &opValue) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value for '%s'", opName);
        return ERR;
    }

    if (opValue.num != opCode)
    {
        AML_DEBUG_ERROR(state, "Invalid %s op: 0x%x", opName, opValue.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_qword_data_t source;
    if (aml_operand_read(state, node, &source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand for '%s'", opName);
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, node, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target for '%s'", opName);
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, op(source)) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init result for '%s'", opName);
        return ERR;
    }

    assert(result.type == AML_DATA_INTEGER);

    // Target is optional
    if (target != NULL)
    {
        if (aml_convert_and_store(&result, target) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result for '%s'", opName);
            return ERR;
        }
    }

    if (out != NULL)
    {
        if (aml_node_clone(&result, out) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to clone result to out for '%s'", opName);
            return ERR;
        }
    }

    aml_node_deinit(&result);
    return 0;
}

/**
 * Helper for reading and executing a structure in the format `OpCode Operand Operand Target`.
 * If `checkDivZero` is true, the function will check for division by zero and return an error if it occurs.
 */
static inline uint64_t aml_binary_op_read(aml_state_t* state, aml_node_t* node, aml_node_t* out, aml_value_num_t opCode,
    const char* opName, aml_binary_op_t op, bool checkDivZero)
{
    aml_value_t opValue;
    if (aml_value_read(state, &opValue) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value for '%s'", opName);
        return ERR;
    }

    if (opValue.num != opCode)
    {
        AML_DEBUG_ERROR(state, "Invalid %s op: 0x%x", opName, opValue.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_qword_data_t source1;
    if (aml_operand_read(state, node, &source1) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read first operand for '%s'", opName);
        return ERR;
    }

    aml_qword_data_t source2;
    if (aml_operand_read(state, node, &source2) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read second operand for '%s'", opName);
        return ERR;
    }

    if (checkDivZero && source2 == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero in '%s'", opName);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, node, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target for '%s'", opName);
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
    if (aml_node_init_integer(&result, op(source1, source2)) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init result for '%s'", opName);
        return ERR;
    }

    assert(result.type == AML_DATA_INTEGER);

    // Target is allowed to be null
    if (target != NULL)
    {
        if (aml_convert_and_store(&result, target) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result for '%s'", opName);
            return ERR;
        }
    }

    if (out != NULL)
    {
        if (aml_node_clone(&result, out) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to init out for '%s'", opName);
            return ERR;
        }
    }

    aml_node_deinit(&result);
    return 0;
}

static inline uint64_t aml_op_add(uint64_t a, uint64_t b)
{
    return a + b;
}

static inline uint64_t aml_op_sub(uint64_t a, uint64_t b)
{
    return a - b;
}

static inline uint64_t aml_op_mul(uint64_t a, uint64_t b)
{
    return a * b;
}

static inline uint64_t aml_op_mod(uint64_t a, uint64_t b)
{
    return a % b;
}

static inline uint64_t aml_op_and(uint64_t a, uint64_t b)
{
    return a & b;
}

static inline uint64_t aml_op_nand(uint64_t a, uint64_t b)
{
    return ~(a & b);
}

static inline uint64_t aml_op_or(uint64_t a, uint64_t b)
{
    return a | b;
}

static inline uint64_t aml_op_nor(uint64_t a, uint64_t b)
{
    return ~(a | b);
}

static inline uint64_t aml_op_xor(uint64_t a, uint64_t b)
{
    return a ^ b;
}

static inline uint64_t aml_op_not(uint64_t a)
{
    return ~a;
}

uint64_t aml_buffer_size_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out)
{
    if (aml_term_arg_read_integer(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_buffer_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t bufferOp;
    if (aml_value_read(state, &bufferOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (bufferOp.num != AML_BUFFER_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid buffer op: 0x%x", bufferOp.num);
        errno = EILSEQ;
        return ERR;
    }

    const uint8_t* start = state->current;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pkg length");
        return ERR;
    }

    const uint8_t* end = start + pkgLength;

    aml_qword_data_t bufferSize;
    if (aml_buffer_size_read(state, node, &bufferSize) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read buffer size");
        return ERR;
    }

    uint64_t availableBytes = (uint64_t)(end - state->current);

    if (aml_node_init_buffer(out, state->current, availableBytes, bufferSize) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init allocated buffer");
        return ERR;
    }

    aml_state_advance(state, availableBytes);
    return 0;
}

uint64_t aml_term_arg_list_read(aml_state_t* state, aml_node_t* node, uint64_t argCount, aml_term_arg_list_t* out)
{
    if (argCount > AML_MAX_ARGS)
    {
        AML_DEBUG_ERROR(state, "Too many arguments: %lu", argCount);
        errno = EILSEQ;
        return ERR;
    }

    out->count = 0;
    for (uint64_t i = 0; i < argCount; i++)
    {
        out->args[i] = AML_NODE_CREATE;
        if (aml_term_arg_read(state, node, &out->args[i]) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read TermArg %lu", i);
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_deinit(&out->args[j]);
            }
            return ERR;
        }
        out->count++;
    }

    return 0;
}

uint64_t aml_method_invocation_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_name_string_t nameString;
    aml_node_t* target = NULL;
    if (aml_name_string_read_and_resolve(state, node, &target, AML_RESOLVE_NONE, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target name string");
        return ERR;
    }

    if (target->type == AML_DATA_METHOD)
    {
        aml_term_arg_list_t args = {0};
        if (aml_term_arg_list_read(state, node, target->method.flags.argCount, &args) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read method arguments");
            return ERR;
        }

        if (aml_method_evaluate(target, &args, out) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to evaluate method");
            for (uint64_t i = 0; i < args.count; i++)
            {
                aml_node_deinit(&args.args[i]);
            }
            return ERR;
        }

        for (uint64_t i = 0; i < args.count; i++)
        {
            aml_node_deinit(&args.args[i]);
        }

        return 0;
    }

    if (aml_node_init_object_reference(out, target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init object reference");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_cond_ref_of_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t condRefOfOp;
    if (aml_value_read(state, &condRefOfOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (condRefOfOp.num != AML_COND_REF_OF_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid cond ref of op: 0x%x", condRefOfOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* source = NULL;
    if (aml_super_name_read_and_resolve(state, node, &source, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve super name");
        return ERR;
    }

    aml_node_t* result = NULL;
    if (aml_target_read_and_resolve(state, node, &result, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve target");
        return ERR;
    }

    if (source == NULL)
    {
        // Return false since the source did not resolve to an object.
        if (aml_node_init_integer(out, 0) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init false integer");
            return ERR;
        }
        return 0;
    }

    if (result == NULL)
    {
        // Return true since source resolved to an object and result dident so we dont need to store anything.
        if (aml_node_init_integer(out, 1) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to init true integer");
            return ERR;
        }
        return 0;
    }

    // Store a reference to source in the result and return true.

    if (aml_node_init_object_reference(result, source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init ObjectReference in result");
        return ERR;
    }

    if (aml_node_init_integer(out, 1) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init true integer");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_store_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t storeOp;
    if (aml_value_read(state, &storeOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (storeOp.num != AML_STORE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid store op: 0x%x", storeOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t source = AML_NODE_CREATE;
    if (aml_term_arg_read(state, node, &source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    aml_node_t* destination = NULL;
    if (aml_super_name_read_and_resolve(state, node, &destination, AML_RESOLVE_NONE, NULL) == ERR)
    {
        aml_node_deinit(&source);
        AML_DEBUG_ERROR(state, "Failed to read super name");
        return ERR;
    }

    if (aml_convert_and_store(&source, destination) == ERR)
    {
        aml_node_deinit(&source);
        AML_DEBUG_ERROR(state, "Failed to store value");
        return ERR;
    }

    if (out != NULL)
    {
        if (aml_node_clone(&source, out) == ERR)
        {
            aml_node_deinit(&source);
            AML_DEBUG_ERROR(state, "Failed to clone source to out");
            return ERR;
        }
    }
    aml_node_deinit(&source);
    return 0;
}

uint64_t aml_operand_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out)
{
    if (aml_term_arg_read_integer(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_dividend_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out)
{
    if (aml_term_arg_read_integer(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_divisor_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out)
{
    if (aml_term_arg_read_integer(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_remainder_read(aml_state_t* state, aml_node_t* node, aml_node_t** out)
{
    if (aml_target_read_and_resolve(state, node, out, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }
    return 0;
}

uint64_t aml_quotient_read(aml_state_t* state, aml_node_t* node, aml_node_t** out)
{
    if (aml_target_read_and_resolve(state, node, out, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_add_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_ADD_OP, "add", aml_op_add, false);
}

uint64_t aml_def_subtract_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_SUBTRACT_OP, "subtract", aml_op_sub, false);
}

uint64_t aml_def_multiply_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_MULTIPLY_OP, "multiply", aml_op_mul, false);
}

uint64_t aml_def_divide_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t divOp;
    if (aml_value_read_no_ext(state, &divOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (divOp.num != AML_DIVIDE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid divide op: 0x%x", divOp.num);
        errno = EILSEQ;
        return ERR;
    }

    uint64_t result = ERR;

    aml_qword_data_t dividend;
    if (aml_dividend_read(state, node, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read dividend");
        return ERR;
    }

    aml_qword_data_t divisor;
    if (aml_divisor_read(state, node, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read divisor");
        return ERR;
    }

    if (divisor == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero");
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* remainderDest = NULL;
    if (aml_remainder_read(state, node, &remainderDest) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read remainder");
        return ERR;
    }

    aml_node_t* quotientDest = NULL;
    if (aml_quotient_read(state, node, &quotientDest) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read quotient");
        return ERR;
    }

    aml_node_t remainder = AML_NODE_CREATE;
    if (aml_node_init_integer(&remainder, dividend % divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init remainder");
        return ERR;
    }

    aml_node_t quotient = AML_NODE_CREATE;
    if (aml_node_init_integer(&quotient, dividend / divisor) == ERR)
    {
        aml_node_deinit(&remainder);
        AML_DEBUG_ERROR(state, "Failed to init quotient");
        return ERR;
    }

    if (aml_convert_and_store(&quotient, quotientDest) == ERR)
    {
        aml_node_deinit(&remainder);
        aml_node_deinit(&quotient);
        AML_DEBUG_ERROR(state, "Failed to store quotient");
        return ERR;
    }

    if (aml_convert_and_store(&remainder, remainderDest) == ERR)
    {
        aml_node_deinit(&remainder);
        aml_node_deinit(&quotient);
        AML_DEBUG_ERROR(state, "Failed to store remainder");
        return ERR;
    }

    aml_node_deinit(&remainder);

    if (out != NULL)
    {
        if (aml_node_clone(&quotient, out) == ERR)
        {
            aml_node_deinit(&quotient);
            AML_DEBUG_ERROR(state, "Failed to clone quotient to out");
            return ERR;
        }
    }

    aml_node_deinit(&quotient);

    return 0;
}

uint64_t aml_def_mod_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_MOD_OP, "mod", aml_op_mod, true);
}

uint64_t aml_def_and_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_AND_OP, "and", aml_op_and, false);
}

uint64_t aml_def_nand_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_NAND_OP, "nand", aml_op_nand, false);
}

uint64_t aml_def_or_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_OR_OP, "or", aml_op_or, false);
}

uint64_t aml_def_nor_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_NOR_OP, "nor", aml_op_nor, false);
}

uint64_t aml_def_xor_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_binary_op_read(state, node, out, AML_XOR_OP, "xor", aml_op_xor, false);
}

uint64_t aml_def_not_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    return aml_unary_op_read(state, node, out, AML_NOT_OP, "not", aml_op_not);
}

uint64_t aml_shift_count_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out)
{
    if (aml_term_arg_read_integer(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_shift_left_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t shlOp;
    if (aml_value_read(state, &shlOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (shlOp.num != AML_SHIFT_LEFT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid shift left op: 0x%x", shlOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_qword_data_t source;
    if (aml_operand_read(state, node, &source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand");
        return ERR;
    }

    aml_qword_data_t shiftCount;
    if (aml_shift_count_read(state, node, &shiftCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read shift count");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, node, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    // C will zero the least significant bits
    if (shiftCount >= sizeof(source) * 8)
    {
        source = 0;
    }
    else
    {
        source <<= shiftCount;
    }

    aml_node_t temp = AML_NODE_CREATE;
    if (aml_node_init_integer(&temp, source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init temp node");
        return ERR;
    }

    // Target is allowed to be null
    if (target != NULL)
    {
        if (aml_convert_and_store(&temp, target) == ERR)
        {
            aml_node_deinit(&temp);
            AML_DEBUG_ERROR(state, "Failed to store result");
            return ERR;
        }
    }

    if (out != NULL)
    {
        if (aml_node_clone(&temp, out) == ERR)
        {
            aml_node_deinit(&temp);
            AML_DEBUG_ERROR(state, "Failed to clone source to out");
            return ERR;
        }
    }

    aml_node_deinit(&temp);
    return 0;
}

uint64_t aml_def_shift_right_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t shrOp;
    if (aml_value_read(state, &shrOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (shrOp.num != AML_SHIFT_RIGHT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid shift right op: 0x%x", shrOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_qword_data_t source;
    if (aml_operand_read(state, node, &source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read operand");
        return ERR;
    }

    aml_qword_data_t shiftCount;
    if (aml_shift_count_read(state, node, &shiftCount) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read shift count");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, node, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    // C will zero the most significant bits
    if (shiftCount >= sizeof(source) * 8)
    {
        source = 0;
    }
    else
    {
        source >>= shiftCount;
    }

    aml_node_t temp = AML_NODE_CREATE;
    if (aml_node_init_integer(&temp, source) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init temp node");
        return ERR;
    }

    // Target is allowed to be null
    if (target != NULL)
    {
        if (aml_convert_and_store(&temp, target) == ERR)
        {
            aml_node_deinit(&temp);
            AML_DEBUG_ERROR(state, "Failed to store result");
            return ERR;
        }
    }

    if (out != NULL)
    {
        if (aml_node_clone(&temp, out) == ERR)
        {
            aml_node_deinit(&temp);
            AML_DEBUG_ERROR(state, "Failed to clone source to out");
            return ERR;
        }
    }

    aml_node_deinit(&temp);
    return 0;
}

uint64_t aml_def_increment_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t incOp;
    if (aml_value_read(state, &incOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (incOp.num != AML_INCREMENT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid increment op: 0x%x", incOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* addend;
    if (aml_super_name_read_and_resolve(state, node, &addend, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read super name");
        return ERR;
    }

    aml_node_t value = AML_NODE_CREATE;
    if (aml_convert_to_integer(addend, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert addend to integer");
        return ERR;
    }

    assert(value.type == AML_DATA_INTEGER);

    value.integer.value++;

    if (aml_convert_and_store(&value, addend) == ERR)
    {
        aml_node_deinit(&value);
        AML_DEBUG_ERROR(state, "Failed to store incremented value");
        return ERR;
    }

    if (out != NULL)
    {
        if (aml_node_clone(&value, out) == ERR)
        {
            aml_node_deinit(&value);
            AML_DEBUG_ERROR(state, "Failed to clone value to out");
            return ERR;
        }
    }

    aml_node_deinit(&value);
    return 0;
}

uint64_t aml_def_decrement_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t decOp;
    if (aml_value_read(state, &decOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (decOp.num != AML_DECREMENT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid decrement op: 0x%x", decOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* subtrahend;
    if (aml_super_name_read_and_resolve(state, node, &subtrahend, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read super name");
        return ERR;
    }

    aml_node_t value = AML_NODE_CREATE;
    if (aml_convert_to_integer(subtrahend, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to convert subtrahend to integer");
        return ERR;
    }

    assert(value.type == AML_DATA_INTEGER);

    value.integer.value--;

    if (aml_convert_and_store(&value, subtrahend) == ERR)
    {
        aml_node_deinit(&value);
        AML_DEBUG_ERROR(state, "Failed to store decremented value");
        return ERR;
    }

    if (out != NULL)
    {
        if (aml_node_clone(&value, out) == ERR)
        {
            aml_node_deinit(&value);
            AML_DEBUG_ERROR(state, "Failed to clone value to out");
            return ERR;
        }
    }

    aml_node_deinit(&value);
    return 0;
}

uint64_t aml_obj_reference_read(aml_state_t* state, aml_node_t* node, aml_node_t** out)
{
    aml_node_t termArg = AML_NODE_CREATE;
    if (aml_term_arg_read(state, node, &termArg) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    if (termArg.type == AML_DATA_OBJECT_REFERENCE)
    {
        *out = termArg.objectReference.target;
        aml_node_deinit(&termArg);
        return 0;
    }
    else if (termArg.type == AML_DATA_STRING)
    {
        aml_node_t* target = aml_node_find(termArg.string.content, node);
        if (target == NULL)
        {
            aml_node_deinit(&termArg);
            AML_DEBUG_ERROR(state, "Failed to find target node '%s'", termArg.string.content);
            errno = EILSEQ;
            return ERR;
        }
        aml_node_deinit(&termArg);

        *out = target;
        return 0;
    }

    aml_node_deinit(&termArg);
    AML_DEBUG_ERROR(state, "Invalid TermArg type: %u", termArg.type);
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_def_deref_of_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t derefOfOp;
    if (aml_value_read(state, &derefOfOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (derefOfOp.num != AML_DEREF_OF_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid deref of op: 0x%x", derefOfOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* objRef = NULL;
    if (aml_obj_reference_read(state, node, &objRef) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read ObjectReference");
        return ERR;
    }

    if (objRef == NULL)
    {
        AML_DEBUG_ERROR(state, "ObjectReference is a null reference");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_node_clone(objRef, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to clone ObjectReference to out");
        return ERR;
    }

    return 0;
}

uint64_t aml_buff_pkg_str_obj_read(aml_state_t* state, aml_node_t* node, aml_node_t** out)
{
    aml_node_t termArg = AML_NODE_CREATE;
    if (aml_term_arg_read(state, node, &termArg) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }

    if (termArg.type != AML_DATA_OBJECT_REFERENCE)
    {
        aml_node_deinit(&termArg);
        AML_DEBUG_ERROR(state, "Invalid TermArg type, expected ObjectReference but got '%s'",
            aml_data_type_to_string(termArg.type));
        errno = EILSEQ;
        return ERR;
    }

    if (termArg.objectReference.target->type == AML_DATA_BUFFER ||
        termArg.objectReference.target->type == AML_DATA_PACKAGE ||
        termArg.objectReference.target->type == AML_DATA_STRING)
    {
        *out = termArg.objectReference.target;
        aml_node_deinit(&termArg);
        return 0;
    }

    aml_node_deinit(&termArg);
    AML_DEBUG_ERROR(state, "Invalid type, expected buffer, package or string but got '%s'",
        aml_data_type_to_string(termArg.objectReference.target->type));
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_index_value_read(aml_state_t* state, aml_node_t* node, aml_qword_data_t* out)
{
    if (aml_term_arg_read_integer(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read TermArg");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_index_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t indexOp;
    if (aml_value_read(state, &indexOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (indexOp.num != AML_INDEX_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid index op: 0x%x", indexOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_node_t* bufferPkgStrObj = NULL;
    if (aml_buff_pkg_str_obj_read(state, node, &bufferPkgStrObj) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read or resolve buffer/pkg/str/obj");
        return ERR;
    }

    aml_qword_data_t index;
    if (aml_index_value_read(state, node, &index) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read index value");
        return ERR;
    }

    aml_node_t* target = NULL;
    if (aml_target_read_and_resolve(state, node, &target, AML_RESOLVE_NONE, NULL) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    aml_node_t result = AML_NODE_CREATE;
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

        if (aml_node_init_object_reference(&result, bufferPkgStrObj->package.elements[index]) == ERR)
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

        if (aml_node_init_object_reference(&result, &bufferPkgStrObj->buffer.byteFields[index]) == ERR)
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

        if (aml_node_init_object_reference(&result, &bufferPkgStrObj->string.byteFields[index]) == ERR)
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
        if (aml_convert_and_store(&result, target) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to store result");
            return ERR;
        }
    }

    if (out != NULL)
    {
        if (aml_node_clone(&result, out) == ERR)
        {
            aml_node_deinit(&result);
            AML_DEBUG_ERROR(state, "Failed to clone result to out");
            return ERR;
        }
    }

    aml_node_deinit(&result);
    return 0;
}

uint64_t aml_expression_opcode_read(aml_state_t* state, aml_node_t* node, aml_node_t* out)
{
    aml_value_t op;
    if (aml_value_peek(state, &op) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    if (op.props->type == AML_VALUE_TYPE_NAME)
    {
        if (aml_method_invocation_read(state, node, out) == ERR)
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
        result = aml_def_buffer_read(state, node, out);
        break;
    case AML_COND_REF_OF_OP:
        result = aml_def_cond_ref_of_read(state, node, out);
        break;
    case AML_STORE_OP:
        result = aml_def_store_read(state, node, out);
        break;
    case AML_ADD_OP:
        result = aml_def_add_read(state, node, out);
        break;
    case AML_SUBTRACT_OP:
        result = aml_def_subtract_read(state, node, out);
        break;
    case AML_MULTIPLY_OP:
        result = aml_def_multiply_read(state, node, out);
        break;
    case AML_DIVIDE_OP:
        result = aml_def_divide_read(state, node, out);
        break;
    case AML_MOD_OP:
        result = aml_def_mod_read(state, node, out);
        break;
    case AML_AND_OP:
        result = aml_def_and_read(state, node, out);
        break;
    case AML_NAND_OP:
        result = aml_def_nand_read(state, node, out);
        break;
    case AML_OR_OP:
        result = aml_def_or_read(state, node, out);
        break;
    case AML_NOR_OP:
        result = aml_def_nor_read(state, node, out);
        break;
    case AML_XOR_OP:
        result = aml_def_xor_read(state, node, out);
        break;
    case AML_NOT_OP:
        result = aml_def_not_read(state, node, out);
        break;
    case AML_SHIFT_LEFT_OP:
        result = aml_def_shift_left_read(state, node, out);
        break;
    case AML_SHIFT_RIGHT_OP:
        result = aml_def_shift_right_read(state, node, out);
        break;
    case AML_INCREMENT_OP:
        result = aml_def_increment_read(state, node, out);
        break;
    case AML_DECREMENT_OP:
        result = aml_def_decrement_read(state, node, out);
        break;
    case AML_DEREF_OF_OP:
        result = aml_def_deref_of_read(state, node, out);
        break;
    case AML_INDEX_OP:
        result = aml_def_index_read(state, node, out);
        break;
    default:
        AML_DEBUG_ERROR(state, "Unknown expression opcode '0x%x'", op.num);
        errno = ENOSYS;
        return ERR;
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read opcode '0x%x'", op.num);
        return ERR;
    }

    return 0;
}
