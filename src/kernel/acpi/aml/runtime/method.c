#include "method.h"

#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"

uint64_t aml_method_evaluate(aml_object_t* method, aml_term_arg_list_t* args, aml_object_t* returnValue)
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

uint64_t aml_method_evaluate_integer(aml_object_t* object, uint64_t* out)
{
    if (object == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type == AML_DATA_INTEGER)
    {
        *out = object->integer.value;
        return 0;
    }

    if (object->type != AML_DATA_METHOD)
    {
        LOG_ERR("object is a '%s', not a method or integer\n", aml_data_type_to_string(object->type));
        errno = EINVAL;
        return ERR;
    }

    aml_object_t returnValue = AML_OBJECT_CREATE(AML_OBJECT_NONE);
    if (aml_method_evaluate(object, NULL, &returnValue) == ERR)
    {
        return ERR;
    }

    if (returnValue.type != AML_DATA_INTEGER)
    {
        errno = EILSEQ;
        return ERR;
    }

    *out = returnValue.integer.value;
    aml_object_deinit(&returnValue);
    return 0;
}
