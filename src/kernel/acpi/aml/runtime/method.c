#include "method.h"

#include "acpi/aml/aml_state.h"

uint64_t aml_method_evaluate(aml_node_t* method, aml_term_arg_list_t* args, aml_node_t* out)
{
    if (method == NULL || method->type != AML_DATA_METHOD)
    {
        errno = EINVAL;
        return ERR;
    }

    if (method->method.implementation != NULL)
    {
        return method->method.implementation(method, args, out);
    }

    aml_state_t state;
    if (aml_state_init(&state, method->method.start, method->method.end, args, out) == ERR)
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
