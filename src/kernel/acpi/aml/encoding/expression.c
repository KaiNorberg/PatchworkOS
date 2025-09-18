#include "expression.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "acpi/aml/runtime/evaluate.h"
#include "arg.h"
#include "package_length.h"
#include "term.h"

uint64_t aml_buffer_size_read(aml_state_t* state, aml_buffer_size_t* out)
{
    aml_data_object_t termArg;
    if (aml_term_arg_read(state, NULL, &termArg, AML_DATA_INTEGER) == ERR)
    {
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
        return ERR;
    }

    if (bufferOp.num != AML_BUFFER_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&bufferOp);
        errno = EILSEQ;
        return ERR;
    }

    aml_address_t start = state->pos;

    aml_pkg_length_t pkgLength;
    if (aml_pkg_length_read(state, &pkgLength) == ERR)
    {
        return ERR;
    }

    aml_address_t end = start + pkgLength;

    aml_buffer_size_t bufferSize;
    if (aml_buffer_size_read(state, &bufferSize) == ERR)
    {
        return ERR;
    }

    uint64_t availableBytes = end - state->pos;

    // If the buffer size matches the end of the package then we can create the buffer in place, otherwise we have to allocate it.
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
        return ERR;
    }

    uint64_t bytesRead = aml_state_read(state, out->content, availableBytes);
    if (bytesRead == ERR)
    {
        aml_buffer_deinit(out);
        return ERR;
    }
    out->length = bytesRead;

    return 0;
}

uint64_t aml_term_arg_list_read(aml_state_t* state, aml_node_t* node, uint8_t argCount, aml_term_arg_list_t* out)
{
    if (argCount > AML_MAX_ARGS)
    {
        AML_DEBUG_INVALID_STRUCTURE("TermArgList: argCount exceeds AML_MAX_ARGS");
        errno = EILSEQ;
        return ERR;
    }

    out->count = 0;
    for (uint8_t i = 0; i < argCount; i++)
    {
        aml_data_object_t* arg = &out->args[i];
        if (aml_term_arg_read(state, node, arg, AML_DATA_ANY) == ERR)
        {
            for (uint8_t j = 0; j < i; j++)
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
        return ERR;
    }

    aml_node_t* target = aml_node_find(&nameString, node);
    if (target == NULL)
    {
        AML_DEBUG_INVALID_STRUCTURE("MethodInvocation: Could not find target");
        errno = EILSEQ;
        return ERR;
    }

    uint8_t argAmount = 0;
    if (target->type == AML_NODE_METHOD)
    {
        argAmount = target->method.flags.argCount;
    }

    aml_term_arg_list_t args = {0};
    if (aml_term_arg_list_read(state, node, argAmount, &args) == ERR)
    {
        return ERR;
    }

    uint64_t result = aml_evaluate(target, out, &args);
    for (uint8_t i = 0; i < args.count; i++)
    {
        aml_data_object_deinit(&args.args[i]);
    }
    return result;
}

uint64_t aml_expression_opcode_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_value_t value;
    if (aml_value_peek(state, &value) == ERR)
    {
        return ERR;
    }

    switch (value.props->type)
    {
    case AML_VALUE_TYPE_EXPRESSION:
        switch (value.num)
        {
        default:
            AML_DEBUG_UNIMPLEMENTED_VALUE(&value);
            errno = ENOSYS;
            return ERR;
        }
    case AML_VALUE_TYPE_NAME:
        return aml_method_invocation_read(state, node, out);
    default:
        AML_DEBUG_UNEXPECTED_VALUE(&value);
        errno = EILSEQ;
        return ERR;
    }
}
