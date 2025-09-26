#include "method.h"

#include "acpi/aml/aml_state.h"

uint64_t aml_method_evaluate(aml_node_t* method, aml_term_arg_list_t* args, aml_node_t* out)
{
    if (method == NULL || method->type != AML_DATA_METHOD || out == NULL)
    {
        return ERR;
    }

    if (method->method.implementation != NULL)
    {
        return method->method.implementation(method, args, out);
    }

    return ERR;
}
