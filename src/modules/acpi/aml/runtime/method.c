#include <kernel/acpi/aml/runtime/method.h>

#include <kernel/acpi/aml/encoding/term.h>
#include <kernel/acpi/aml/runtime/copy.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/log/log.h>

#include <errno.h>

status_t aml_method_invoke(aml_state_t* parentState, aml_method_t* method, aml_object_t** args, aml_object_t** out)
{
    if (method == NULL || parentState == NULL || out == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    uint8_t argCount = 0;
    if (args != NULL)
    {
        while (argCount < AML_MAX_ARGS + 1 && args[argCount] != NULL)
        {
            argCount++;
        }

        if (argCount == AML_MAX_ARGS + 1)
        {
            LOG_ERR("too many arguments, max is %u\n", AML_MAX_ARGS);
            return ERR(ACPI, INVAL);
        }
    }

    if (argCount != method->methodFlags.argCount)
    {
        LOG_ERR("method '%s' expects %u arguments, got %u\n", AML_NAME_TO_STRING(method->name),
            method->methodFlags.argCount, argCount);
        return ERR(ACPI, INVAL);
    }

    if (method->methodFlags.isSerialized)
    {
        status_t status = aml_mutex_acquire(&method->mutex, method->methodFlags.syncLevel, CLOCKS_NEVER);
        if (IS_ERR(status))
        {
            LOG_ERR("could not acquire method mutex\n");
            return status;
        }
    }

    if (method->implementation != NULL)
    {
        aml_object_t* temp = method->implementation(method, args, argCount);
        if (method->methodFlags.isSerialized)
        {
            status_t status = aml_mutex_release(&method->mutex);
            if (IS_ERR(status))
            {
                LOG_ERR("could not release method mutex\n");
                return status;
            }
        }
        *out = temp;
        return OK;
    }

    aml_state_t state;
    status_t status = aml_state_init(&state, args);
    if (IS_ERR(status))
    {
        LOG_ERR("could not initialize AML state\n");
        if (method->methodFlags.isSerialized)
        {
            aml_mutex_release(&method->mutex); // Ignore errors
        }
        return status;
    }

    aml_object_t* methodObj = CONTAINER_OF(method, aml_object_t, method);

    // This is a mess. Just check namespace.h for details.
    aml_overlay_t* highestThatContainsMethod = aml_overlay_find_containing(&parentState->overlay, methodObj);
    if (highestThatContainsMethod == NULL)
    {
        // Should never happen.
        if (method->methodFlags.isSerialized)
        {
            aml_mutex_release(&method->mutex); // Ignore errors
        }
        return ERR(ACPI, IMPL);
    }
    aml_overlay_set_parent(&state.overlay, highestThatContainsMethod);

    // "The current namespace location is assigned to the method package, and all namespace references that occur during
    // control method execution for this package are relative to that location." - Section 19.6.85

    // The method body is just a TermList.
    status = aml_term_list_read(&state, methodObj, method->start, method->end, NULL);
    if (IS_ERR(status))
    {
        LOG_ERR("failed to read method body for method '%s'\n", AML_NAME_TO_STRING(method->name));
        aml_state_deinit(&state);
        if (method->methodFlags.isSerialized)
        {
            aml_mutex_release(&method->mutex); // Ignore errors
        }
        return status;
    }

    if (method->methodFlags.isSerialized)
    {
        status = aml_mutex_release(&method->mutex);
        if (IS_ERR(status))
        {
            LOG_ERR("could not release method mutex\n");
            aml_state_deinit(&state);
            return status;
        }
    }

    status = aml_state_result_get(&state, out);
    aml_state_deinit(&state);
    return status;
}
