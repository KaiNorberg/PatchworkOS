#include "expression.h"

#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "acpi/aml/aml_value.h"
#include "arg.h"
#include "package_length.h"
#include "term.h"

uint64_t aml_buffer_size_read(aml_state_t* state, aml_buffer_size_t* out)
{
    aml_term_arg_t termArg;
    if (aml_term_arg_read(state, NULL, &termArg, AML_DATA_INTEGER) == ERR)
    {
        return ERR;
    }
    *out = termArg.integer;
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

    if (pkgLength < 1)
    {
        AML_DEBUG_INVALID_STRUCTURE("DefBuffer: Buffer length must be at least 1");
        errno = EILSEQ;
        return ERR;
    }

    aml_buffer_size_t bufferSize;
    if (aml_buffer_size_read(state, &bufferSize) == ERR)
    {
        return ERR;
    }

    // TODO: Im not sure why we have both pkgLength and bufferSize, but for now we just check they match. In the future
    // if this causes an error we can figure it out from there.

    aml_address_t end = start + pkgLength;
    if (end != state->pos + bufferSize)
    {
        LOG_ERR("pkgLength: %llu, bufferSize: %llu, calculated end: 0x%llx, actual end: 0x%llx\n", pkgLength,
            bufferSize, state->pos + bufferSize, end);
        AML_DEBUG_INVALID_STRUCTURE("DefBuffer: Mismatch between PkgLength and BufferSize, unsure if this is valid");
        errno = ENOSYS;
        return ERR;
    }

    out->content = (uint8_t*)(state->data + state->pos);
    out->length = bufferSize;
    aml_state_advance(state, bufferSize);
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
        aml_term_arg_t* arg = &out->args[i];
        if (aml_term_arg_read(state, node, arg, AML_DATA_ANY) == ERR)
        {
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
        argAmount = target->data.method.flags.argCount;
    }

    aml_term_arg_list_t args = {0};
    if (aml_term_arg_list_read(state, node, argAmount, &args) == ERR)
    {
        return ERR;
    }

    return aml_node_evaluate(target, out, &args);
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
