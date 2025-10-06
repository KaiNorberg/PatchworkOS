#include "method.h"

#include "acpi/aml/aml_state.h"
#include "acpi/aml/aml_to_string.h"
#include "log/log.h"

#include <errno.h>

uint64_t aml_method_evaluate(aml_method_t* method, uint64_t argCount, aml_object_t** args, aml_object_t* returnValue)
{
    if (method == NULL)
    {
        LOG_ERR("method is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (argCount > AML_MAX_ARGS || argCount != method->methodFlags.argCount)
    {
        LOG_ERR("method '%s' expects %u arguments, got %u\n", AML_OBJECT_GET_NAME(method),
            method->methodFlags.argCount, argCount);
        errno = EINVAL;
        return ERR;
    }

    if (args == NULL && argCount > 0)
    {
        LOG_ERR("method '%s' expects %u arguments, got NULL\n", AML_OBJECT_GET_NAME(method),
            method->methodFlags.argCount);
        errno = EINVAL;
        return ERR;
    }

    uint64_t result = 0;
    if (method->methodFlags.isSerialized)
    {
        if (aml_mutex_acquire(&method->mutex, method->methodFlags.syncLevel, CLOCKS_NEVER) == ERR)
        {
            LOG_ERR("could not acquire method mutex\n");
            return ERR;
        }
    }

    if (method->implementation != NULL)
    {
        result = method->implementation(method, argCount, args, returnValue);
        goto cleanup;
    }

    aml_state_t state;
    if (aml_state_init(&state, method->start, method->end, argCount, args, returnValue) == ERR)
    {
        LOG_ERR("could not initialize AML state\n");
        result = ERR;
        goto cleanup;
    }

    // "The current namespace location is assigned to the method package, and all namespace references that occur during
    // control method execution for this package are relative to that location." - Section 19.6.85

    // The method body is just a TermList.
    result = aml_term_list_read(&state, CONTAINER_OF(method, aml_object_t, method), method->end);

    // "Also notice that all namespace objects created by a method have temporary lifetime. When method execution exits,
    // the created objects will be destroyed." - Section 19.6.85
    aml_state_garbage_collect(&state);

    if (aml_state_deinit(&state) == ERR)
    {
        LOG_ERR("could not deinitialize AML state\n");
        result = ERR;
        goto cleanup;
    }

cleanup:
    if (method->methodFlags.isSerialized)
    {
        if (aml_mutex_release(&method->mutex) == ERR)
        {
            LOG_ERR("could not release method mutex\n");
            result = ERR;
        }
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

    aml_type_t type = object->type;
    if (type == AML_INTEGER)
    {
        *out = object->integer.value;
        return 0;
    }

    if (type != AML_METHOD)
    {
        LOG_ERR("object is a '%s', not a method or integer\n", aml_type_to_string(type));
        errno = EINVAL;
        return ERR;
    }

    aml_object_t* returnValue = aml_object_new(NULL, AML_OBJECT_NONE);
    if (returnValue == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(returnValue);

    if (aml_method_evaluate(&object->method, 0, NULL, returnValue) == ERR)
    {
        return ERR;
    }

    aml_type_t returnType = returnValue->type;
    if (returnType != AML_INTEGER)
    {
        LOG_ERR("method did not return an Integer, returned '%s' instead\n", aml_type_to_string(returnType));
        return ERR;
    }

    *out = returnValue->integer.value;
    return 0;
}
