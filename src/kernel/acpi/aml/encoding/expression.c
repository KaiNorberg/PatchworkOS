#include "expression.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
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
        AML_DEBUG_ERROR(state, "Failed to find target node");
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

    LOG_DEBUG("Reading term arg list for %.*s, expecting %u args\n", 4, target->segment, argAmount);

    aml_term_arg_list_t args = {0};
    if (aml_term_arg_list_read(state, node, argAmount, &args) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to read term arg list");
        return ERR;
    }

    LOG_DEBUG("Evaluating %.*s with %u args\n", 4, target->segment, args.count);

    uint64_t result = aml_evaluate(target, out, &args);
    for (uint8_t i = 0; i < args.count; i++)
    {
        aml_data_object_deinit(&args.args[i]);
    }
    return result;
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

uint64_t aml_expression_opcode_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        AML_DEBUG_ERROR(state, "Failed to peek value");
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_EXPRESSION:
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
        default:
            AML_DEBUG_ERROR(state, "Unknown expression opcode: 0x%x", value.num);
            errno = ENOSYS;
            return ERR;
        }
    case AML_VALUE_TYPE_NAME:
        return aml_method_invocation_read(state, node, out);
    default:
        AML_DEBUG_ERROR(state, "Invalid value type: %d", value.props->type);
        errno = EILSEQ;
        return ERR;
    }
}
