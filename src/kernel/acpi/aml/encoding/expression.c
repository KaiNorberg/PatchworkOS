#include "expression.h"

#include "acpi/aml/aml.h"
#include "acpi/aml/aml_debug.h"
#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_value.h"
#include "arg.h"
#include "package_length.h"
#include "term.h"
#include "object_reference.h"
#include "data_object.h"

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

    uint64_t argAmount = aml_node_get_expected_arg_count(target);
    if (argAmount == ERR)
    {
        AML_DEBUG_INVALID_STRUCTURE("MethodInvocation: Failed to get expected arg count");
        errno = EILSEQ;
        return ERR;
    }

    LOG_DEBUG("Reading term arg list for %.*s, expecting %u args\n", 4, target->segment, argAmount);

    aml_term_arg_list_t args = {0};
    if (aml_term_arg_list_read(state, node, argAmount, &args) == ERR)
    {
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
        return ERR;
    }

    if (condRefOfOp.num != AML_COND_REF_OF_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&condRefOfOp);
        errno = EILSEQ;
        return ERR;
    }

    aml_object_reference_t superObject;
    if (aml_super_name_read(state, node, &superObject) == ERR)
    {
        return ERR;
    }

    aml_object_reference_t target;
    if (aml_target_read(state, node, &target) == ERR)
    {
        return ERR;
    }


    if (superObject.type == AML_OBJECT_REFERENCE_EMPTY)
    {
        // Return false since the SuperName did not resolve to an object.
        return aml_data_object_init_integer(out, 0, 64);
    }

    if (target.type == AML_OBJECT_REFERENCE_EMPTY)
    {
        // Return true since SuperName resolved to an object and Target is a NullName.
        return aml_data_object_init_integer(out, 1, 64);
    }

    // Store reference to SuperObject in the target and return true.

    switch (target.type)
    {
    case AML_OBJECT_REFERENCE_NODE:
    {
        aml_node_t* targetNode = target.node;
        if (targetNode->type != AML_NODE_NAME) // This is probably right... maybe.
        {
            LOG_ERR("CondRefOf: Target is not a Name object\n");
            errno = EILSEQ;
            return ERR;
        }

        aml_data_object_deinit(&targetNode->name.object);
        if (aml_data_object_init_object_reference(out, &superObject) == ERR)
        {
            return ERR;
        }
    }
    break;
    case AML_OBJECT_REFERENCE_DATA_OBJECT:
    {
        aml_data_object_t* targetObject = target.dataObject;
        aml_data_object_deinit(targetObject);
        if (aml_data_object_init_object_reference(targetObject, &superObject) == ERR)
        {
            return ERR;
        }
    }
    break;
    default:
        AML_DEBUG_INVALID_STRUCTURE("CondRefOf: Invalid target type");
        errno = EILSEQ;
        return ERR;
    }

    if (aml_data_object_init_integer(out, 1, 64) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_def_store_read(aml_state_t* state, aml_node_t* node, aml_data_object_t* out)
{
    aml_value_t storeOp;
    if (aml_value_read(state, &storeOp) == ERR)
    {
        return ERR;
    }

    if (storeOp.num != AML_STORE_OP)
    {
        AML_DEBUG_UNEXPECTED_VALUE(&storeOp);
        errno = EILSEQ;
        return ERR;
    }

    aml_data_object_t source;
    if (aml_term_arg_read(state, node, &source, AML_DATA_ANY) == ERR)
    {
        return ERR;
    }

    aml_object_reference_t target;
    if (aml_super_name_read(state, node, &target) == ERR)
    {
        aml_data_object_deinit(&source);
        return ERR;
    }

    if (target.type == AML_OBJECT_REFERENCE_EMPTY)
    {
        AML_DEBUG_INVALID_STRUCTURE("Store: Failed to resolve target");
        aml_data_object_deinit(&source);
        errno = EILSEQ;
        return ERR;
    }

    switch (target.type)
    {
    case AML_OBJECT_REFERENCE_NODE:
    {
        aml_node_t* targetNode = target.node;

        if (aml_store(targetNode, &source) == ERR)
        {
            aml_data_object_deinit(&source);
            return ERR;
        }
    }
    break;
    case AML_OBJECT_REFERENCE_DATA_OBJECT:
    {
        aml_data_object_t* targetObject = target.dataObject;

        aml_data_object_deinit(targetObject);
        *targetObject = source;
    }
    break;
    default:
        AML_DEBUG_INVALID_STRUCTURE("Store: Invalid target type");
        aml_data_object_deinit(&source);
        errno = EILSEQ;
        return ERR;
    }

    *out = source;
    aml_data_object_deinit(&source);
    return 0;
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
        case AML_BUFFER_OP:
        {
            aml_buffer_t buffer;
            if (aml_def_buffer_read(state, &buffer) == ERR)
            {
                return ERR;
            }

            if (aml_data_object_init_buffer(out, &buffer) == ERR)
            {
                aml_buffer_deinit(&buffer);
                return ERR;
            }

            return 0;
        }
        case AML_COND_REF_OF_OP:
            return aml_def_cond_ref_of_read(state, node, out);
        case AML_STORE_OP:
            return aml_def_store_read(state, node, out);
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
