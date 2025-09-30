#include "method.h"

#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"

uint64_t aml_method_evaluate(aml_node_t* method, aml_term_arg_list_t* args, aml_node_t* returnValue)
{
    if (method == NULL || method->type != AML_DATA_METHOD)
    {
        errno = EINVAL;
        return ERR;
    }

    if (method->method.implementation != NULL)
    {
        return method->method.implementation(method, args, returnValue);
    }

    aml_state_t state;
    if (aml_state_init(&state, method->method.start, method->method.end, args, returnValue) == ERR)
    {
        return ERR;
    }

    if (method->method.flags.isSerialized)
    {
        mutex_acquire(&method->method.mutex);
    }

    // The method body is just a TermList.
    uint64_t result = aml_term_list_read(&state, method, method->method.end);

    if (method->method.flags.isSerialized)
    {
        mutex_release(&method->method.mutex);
    }

    aml_state_deinit(&state);
    return result;
}

uint64_t aml_method_evaluate_integer(aml_node_t* node, uint64_t* out)
{
    if (node == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type == AML_DATA_INTEGER)
    {
        *out = node->integer.value;
        return 0;
    }

    if (node->type != AML_DATA_METHOD)
    {
        LOG_ERR("node is a '%s', not a method or integer\n", aml_data_type_to_string(node->type));
        errno = EINVAL;
        return ERR;
    }

    aml_node_t returnValue = AML_NODE_CREATE(AML_NODE_NONE);
    if (aml_method_evaluate(node, NULL, &returnValue) == ERR)
    {
        return ERR;
    }

    if (returnValue.type != AML_DATA_INTEGER)
    {
        errno = EILSEQ;
        return ERR;
    }

    *out = returnValue.integer.value;
    aml_node_deinit(&returnValue);
    return 0;
}
