#include "expression.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "arg.h"
#include "data_object.h"
#include "object_reference.h"
#include "package_length.h"
#include "term.h"

uint64_t aml_buffer_size_read(aml_state_t* state, aml_buffer_size_t* out)
{
    aml_data_object_t termArg;
    if (aml_term_arg_read(state, NULL, &termArg, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg");
        return ERR;
    }

    *out = termArg.integer;
    aml_data_object_deinit(&termArg);

    return 0;
}

uint64_t aml_def_buffer_read(aml_state_t* state, aml_buffer_t* out)
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

    aml_address_t start = state->pos;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read pkg length");
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_buffer_size_t bufferSize;
    if (aml_buffer_size_read(state, &bufferSize) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read buffer size");
        return ERR;
    }

    uint64_t availableBytes = end - state->pos;

    // If the buffer size matches the end of the package then we can create the buffer in place, otherwise we have to
    // allocate it.
    if (availableBytes == bufferSize)
    {
        *out = AML_BUFFER_CREATE_IN_PLACE((uint8_t*)(state->data + state->pos), bufferSize);
        aml_address_t offset = end - state->pos;
        aml_state_advance(state, offset);
        return 0;
    }

    *out = AML_BUFFER_CREATE(bufferSize);
    if (out->content == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to allocate memory for buffer");
        return ERR;
    }

    uint64_t bytesRead = aml_state_read(state, out->content, availableBytes);
    if (bytesRead == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read buffer content");
        aml_buffer_deinit(out);
        return ERR;
    }
    out->length = bytesRead;

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
        aml_data_object_t* arg = &out->args[i];
        if (aml_term_arg_read(state, node, arg, AML_DATA_ANY) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read term arg %lu", i);
            for (uint64_t j = 0; j < i; j++)
            {
                aml_data_object_deinit(&out->args[j]);
            }
            return ERR;
        }
        out->count++;
    }

    return 0;
}

uint64_t aml_method_invocation_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_name_string_t nameString;
    if (aml_name_string_read(state, &nameString) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read name string");
        return ERR;
    }

    aml_node_t* target = aml_node_find(&nameString, node);
    if (target == NULL)
    {
        AML_DEBUG_ERROR(state, "Failed to find target node '%s'", aml_name_string_to_string(&nameString));
        errno = EILSEQ;
        return ERR;
    }

    uint64_t argAmount = aml_node_get_expected_arg_count(target);
    if (argAmount == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to get expected arg count");
        errno = EILSEQ;
        return ERR;
    }

    aml_term_arg_list_t args = {0};
    if (aml_term_arg_list_read(state, node, argAmount, &args) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg list");
        return ERR;
    }

    LOG_DEBUG("evaluating '%.*s' with %u args\n", AML_NAME_LENGTH, target->segment, args.count);

    uint64_t result = aml_evaluate(target, out, &args);
    for (uint8_t i = 0; i < args.count; i++)
    {
        aml_data_object_deinit(&args.args[i]);
    }

    if (result == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to evaluate method '%.*s'", AML_NAME_LENGTH, target->segment);
        return ERR;
    }
    return 0;
}

uint64_t aml_def_cond_ref_of_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
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

    aml_object_reference_t superObject;
    if (aml_super_name_read(state, node, &superObject) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read super name");
        return ERR;
    }

    aml_object_reference_t target;
    if (aml_target_read(state, node, &target) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    if (aml_object_reference_is_null(&superObject))
    {
        // Return false since the SuperName did not resolve to an object.
        return aml_data_object_init_integer(out, 0, 64);
    }

    if (aml_object_reference_is_null(&target))
    {
        // Return true since SuperName resolved to an object and Target is a NullName.
        return aml_data_object_init_integer(out, 1, 64);
    }

    // Store reference to SuperObject in the target and return true.

    aml_data_object_t temp;
    if (aml_data_object_init_object_reference(&temp, &superObject) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init object reference");
        return ERR;
    }

    if (aml_store(aml_object_reference_deref(&target), &temp) == ERR)
    {
        aml_data_object_deinit(&temp);
        AML_DEBUG_ERROR(state, "Failed to store reference");
        return ERR;
    }

    aml_data_object_deinit(&temp);

    if (aml_data_object_init_integer(out, 1, 64) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init integer");
        return ERR;
    }

    return 0;
}

uint64_t aml_def_store_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
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

    aml_data_object_t source;
    if (aml_term_arg_read(state, node, &source, AML_DATA_ANY) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg");
        return ERR;
    }

    aml_object_reference_t target;
    if (aml_super_name_read(state, node, &target) == ERR)
    {
        aml_data_object_deinit(&source);
        AML_DEBUG_ERROR(state, "Failed to read super name");
        return ERR;
    }

    if (aml_object_reference_is_null(&target))
    {
        aml_data_object_deinit(&source);
        AML_DEBUG_ERROR(state, "Target is a null reference");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_store(aml_object_reference_deref(&target), &source) == ERR)
    {
        aml_data_object_deinit(&source);
        AML_DEBUG_ERROR(state, "Failed to store value");
        return ERR;
    }

    *out = source; // Transfer ownership
    return 0;
}

uint64_t aml_operand_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    if (aml_term_arg_read(state, node, out, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg");
        return ERR;
    }
    return 0;
}

uint64_t aml_dividend_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    if (aml_term_arg_read(state, node, out, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg");
        return ERR;
    }
    return 0;
}

uint64_t aml_divisor_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    if (aml_term_arg_read(state, node, out, AML_DATA_INTEGER) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg");
        return ERR;
    }
    return 0;
}

uint64_t aml_remainder_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out)
{
    if (aml_target_read(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }
    return 0;
}

uint64_t aml_quotient_read(aml_state_t* state, aml_node_t* node, aml_object_reference_t* out)
{
    if (aml_target_read(state, node, out) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }
    return 0;
}

uint64_t aml_def_add_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_value_t addOp;
    if (aml_value_read_no_ext(state, &addOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (addOp.num != AML_ADD_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid add op: 0x%x", addOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_data_object_t addend1;
    if (aml_operand_read(state, node, &addend1) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read first addend");
        return ERR;
    }

    aml_data_object_t addend2;
    if (aml_operand_read(state, node, &addend2) == ERR)
    {
        aml_data_object_deinit(&addend1);
        AML_DEBUG_ERROR(state, "Failed to read second addend");
        return ERR;
    }

    assert(addend1.type == AML_DATA_INTEGER);
    assert(addend2.type == AML_DATA_INTEGER);

    aml_object_reference_t target;
    if (aml_target_read(state, node, &target) == ERR)
    {
        aml_data_object_deinit(&addend1);
        aml_data_object_deinit(&addend2);
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    // We dont care about overflow.
    uint8_t bitWidth = MAX(addend1.meta.bitWidth, addend2.meta.bitWidth);

    aml_data_object_t result;
    if (aml_data_object_init_integer(&result, addend1.integer + addend2.integer, bitWidth) == ERR)
    {
        aml_data_object_deinit(&addend1);
        aml_data_object_deinit(&addend2);
        AML_DEBUG_ERROR(state, "Failed to init result");
        return ERR;
    }

    aml_data_object_deinit(&addend1);
    aml_data_object_deinit(&addend2);

    if (aml_store(aml_object_reference_deref(&target), &result) == ERR)
    {
        aml_data_object_deinit(&result);
        AML_DEBUG_ERROR(state, "Failed to store result");
        return ERR;
    }

    if (out != NULL)
    {
        *out = result; // Transfer ownership
        return 0;
    }
    aml_data_object_deinit(&result);
    return 0;
}

uint64_t aml_def_subtract_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_value_t subOp;
    if (aml_value_read_no_ext(state, &subOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (subOp.num != AML_SUBTRACT_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid subtract op: 0x%x", subOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_data_object_t minuend;
    if (aml_operand_read(state, node, &minuend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read minuend");
        return ERR;
    }

    aml_data_object_t subtrahend;
    if (aml_operand_read(state, node, &subtrahend) == ERR)
    {
        aml_data_object_deinit(&minuend);
        AML_DEBUG_ERROR(state, "Failed to read subtrahend");
        return ERR;
    }

    assert(minuend.type == AML_DATA_INTEGER);
    assert(subtrahend.type == AML_DATA_INTEGER);

    aml_object_reference_t target;
    if (aml_target_read(state, node, &target) == ERR)
    {
        aml_data_object_deinit(&minuend);
        aml_data_object_deinit(&subtrahend);
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    // We dont care about underflow.
    uint8_t bitWidth = MAX(minuend.meta.bitWidth, subtrahend.meta.bitWidth);

    aml_data_object_t result;
    if (aml_data_object_init_integer(&result, minuend.integer - subtrahend.integer, bitWidth) == ERR)
    {
        aml_data_object_deinit(&minuend);
        aml_data_object_deinit(&subtrahend);
        AML_DEBUG_ERROR(state, "Failed to init result");
        return ERR;
    }

    aml_data_object_deinit(&minuend);
    aml_data_object_deinit(&subtrahend);

    if (aml_store(aml_object_reference_deref(&target), &result) == ERR)
    {
        aml_data_object_deinit(&result);
        AML_DEBUG_ERROR(state, "Failed to store result");
        return ERR;
    }

    if (out != NULL)
    {
        *out = result; // Transfer ownership
        return 0;
    }
    aml_data_object_deinit(&result);
    return 0;
}

uint64_t aml_def_multiply_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_value_t mulOp;
    if (aml_value_read_no_ext(state, &mulOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        return ERR;
    }

    if (mulOp.num != AML_MULTIPLY_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid multiply op: 0x%x", mulOp.num);
        errno = EILSEQ;
        return ERR;
    }

    aml_data_object_t multiplcand;
    if (aml_operand_read(state, node, &multiplcand) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read multiplcand");
        return ERR;
    }

    aml_data_object_t multiplier;
    if (aml_operand_read(state, node, &multiplier) == ERR)
    {
        aml_data_object_deinit(&multiplcand);
        AML_DEBUG_ERROR(state, "Failed to read second factor");
        return ERR;
    }

    assert(multiplcand.type == AML_DATA_INTEGER);
    assert(multiplier.type == AML_DATA_INTEGER);

    aml_object_reference_t target;
    if (aml_target_read(state, node, &target) == ERR)
    {
        aml_data_object_deinit(&multiplcand);
        aml_data_object_deinit(&multiplier);
        AML_DEBUG_ERROR(state, "Failed to read target");
        return ERR;
    }

    // We dont care about overflow.
    uint8_t bitWidth = MAX(multiplcand.meta.bitWidth, multiplier.meta.bitWidth);

    aml_data_object_t result;
    if (aml_data_object_init_integer(&result, multiplcand.integer * multiplier.integer, bitWidth) == ERR)
    {
        aml_data_object_deinit(&multiplcand);
        aml_data_object_deinit(&multiplier);
        AML_DEBUG_ERROR(state, "Failed to init result");
        return ERR;
    }

    aml_data_object_deinit(&multiplcand);
    aml_data_object_deinit(&multiplier);

    if (aml_store(aml_object_reference_deref(&target), &result) == ERR)
    {
        aml_data_object_deinit(&result);
        AML_DEBUG_ERROR(state, "Failed to store result");
        return ERR;
    }

    if (out != NULL)
    {
        *out = result; // Transfer ownership
        return 0;
    }
    aml_data_object_deinit(&result);
    return 0;
}

uint64_t aml_def_divide_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_value_t divOp;
    aml_data_object_t dividend = {0};
    aml_data_object_t divisor = {0};
    aml_object_reference_t remainderRef = {0};
    aml_object_reference_t quotientRef = {0};
    aml_data_object_t remainder = {0};
    aml_data_object_t quotient = {0};
    uint64_t result = ERR;

    if (aml_value_read_no_ext(state, &divOp) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read value");
        goto cleanup;
    }

    if (divOp.num != AML_DIVIDE_OP)
    {
        AML_DEBUG_ERROR(state, "Invalid divide op: 0x%x", divOp.num);
        errno = EILSEQ;
        goto cleanup;
    }

    if (aml_dividend_read(state, node, &dividend) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read dividend");
        goto cleanup;
    }

    if (aml_divisor_read(state, node, &divisor) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read divisor");
        goto cleanup_dividend;
    }

    assert(dividend.type == AML_DATA_INTEGER);
    assert(divisor.type == AML_DATA_INTEGER);

    if (divisor.integer == 0)
    {
        AML_DEBUG_ERROR(state, "Division by zero");
        errno = EILSEQ;
        goto cleanup_divisor;
    }

    if (aml_remainder_read(state, node, &remainderRef) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read remainder");
        goto cleanup_divisor;
    }

    if (aml_quotient_read(state, node, &quotientRef) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read quotient");
        goto cleanup_remainder_ref;
    }

    uint8_t bitWidth = MAX(dividend.meta.bitWidth, divisor.meta.bitWidth);

    if (aml_data_object_init_integer(&remainder, dividend.integer % divisor.integer, bitWidth) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init remainder");
        goto cleanup_quotient_ref;
    }

    if (aml_data_object_init_integer(&quotient, dividend.integer / divisor.integer, bitWidth) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to init quotient");
        goto cleanup_remainder;
    }

    aml_data_object_deinit(&dividend);
    aml_data_object_deinit(&divisor);

    if (aml_store(aml_object_reference_deref(&quotientRef), &quotient) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to store quotient");
        goto cleanup_remainder;
    }

    aml_object_reference_deinit(&quotientRef);

    if (aml_store(aml_object_reference_deref(&remainderRef), &remainder) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to store remainder");
        goto cleanup_remainder;
    }

    aml_object_reference_deinit(&remainderRef);
    aml_data_object_deinit(&remainder);

    if (out != NULL)
    {
        *out = quotient; // Transfer ownership
    }
    else
    {
        aml_data_object_deinit(&quotient);
    }

    return 0;

cleanup_remainder:
    aml_data_object_deinit(&remainder);
cleanup_quotient_ref:
    aml_object_reference_deinit(&quotientRef);
cleanup_remainder_ref:
    aml_object_reference_deinit(&remainderRef);
cleanup_divisor:
    aml_data_object_deinit(&divisor);
cleanup_dividend:
    aml_data_object_deinit(&dividend);
cleanup:
    return result;
}

uint64_t aml_expression_opcode_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    if (value.props->type == AML_VALUE_TYPE_NAME)
    {
        return aml_method_invocation_read(state, node, out);
    }

    switch (value.num)
    {
    case AML_BUFFER_OP:
    {
        aml_buffer_t buffer;
        if (aml_def_buffer_read(state, &buffer) == ERR)
        {
            AML_DEBUG_ERROR(state, "Failed to read buffer");
            return ERR;
        }

        if (aml_data_object_init_buffer(out, &buffer) == ERR)
        {
            aml_buffer_deinit(&buffer);
            AML_DEBUG_ERROR(state, "Failed to init buffer");
            return ERR;
        }

        return 0;
    }
    case AML_COND_REF_OF_OP:
        return aml_def_cond_ref_of_read(state, node, out);
    case AML_STORE_OP:
        return aml_def_store_read(state, node, out);
    case AML_ADD_OP:
        return aml_def_add_read(state, node, out);
    case AML_SUBTRACT_OP:
        return aml_def_subtract_read(state, node, out);
    case AML_MULTIPLY_OP:
        return aml_def_multiply_read(state, node, out);
    case AML_DIVIDE_OP:
        return aml_def_divide_read(state, node, out);
    default:
        AML_DEBUG_ERROR(state, "Unknown expression opcode: 0x%x", value.num);
        errno = ENOSYS;
        return ERR;
    }
}
