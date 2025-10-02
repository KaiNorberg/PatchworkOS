#include "method.h"

#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "log/log.h"

#include <errno.h>

uint64_t aml_method_evaluate(aml_object_t* method, uint64_t argCount, aml_object_t** args, aml_object_t* returnValue)
{
    if (method == NULL || method->type != AML_DATA_METHOD)
    {
        errno = EINVAL;
        return ERR;
    }

    if (argCount > AML_MAX_ARGS || argCount != method->method.flags.argCount)
    {
        errno = EINVAL;
        return ERR;
    }

    if (args == NULL && argCount > 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (method->method.implementation != NULL)
    {
        return method->method.implementation(method, argCount, args, returnValue);
    }

    aml_state_t state;
    if (aml_state_init(&state, method->method.start, method->method.end, argCount, args, returnValue) == ERR)
    {
        return ERR;
    }

    if (method->method.flags.isSerialized)
    {
        mutex_acquire(&method->method.mutex);
    }

    // "The current namespace location is assigned to the method package, and all namespace references that occur during
    // control method execution for this package are relative to that location." - Section 19.6.85

    // The method body is just a TermList.
    uint64_t result = aml_term_list_read(&state, method, method->method.end);

    // "Also notice that all namespace objects created by a method have temporary lifetime. When method execution exits,
    // the created objects will be destroyed." - Section 19.6.85
    aml_object_t* child = NULL;
    aml_object_t* temp = NULL;
    LIST_FOR_EACH_SAFE(child, temp, &method->children, entry)
    {
        aml_object_free(child);
    }

    if (method->method.flags.isSerialized)
    {
        mutex_release(&method->method.mutex);
    }

    if (aml_state_deinit(&state) == ERR)
    {
        LOG_ERR("failed to deinitialize AML state\n");
        return ERR;
    }
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
    if (aml_method_evaluate(object, 0, NULL, &returnValue) == ERR)
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
