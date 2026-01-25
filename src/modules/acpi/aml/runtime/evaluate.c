#include <kernel/acpi/aml/encoding/arg.h>
#include <kernel/acpi/aml/runtime/evaluate.h>

#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/acpi/aml/runtime/method.h>
#include <kernel/acpi/aml/state.h>

aml_object_t* aml_evaluate(aml_state_t* state, aml_object_t* object, aml_type_t targetTypes)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (state == NULL)
    {
        aml_state_t tempState;
        if (aml_state_init(&tempState, NULL) == _FAIL)
        {
            return NULL;
        }

        aml_object_t* result = aml_evaluate(&tempState, object, targetTypes);
        aml_state_deinit(&tempState);
        return result;
    }

    if (object->type & targetTypes)
    {
        return REF(object);
    }

    if (object->type == AML_METHOD)
    {
        aml_object_t* result = aml_method_invoke(state, &object->method, NULL);
        if (result == NULL)
        {
            return NULL;
        }
        UNREF_DEFER(result);

        aml_object_t* converted = NULL;
        if (aml_convert_source(state, result, &converted, targetTypes) == _FAIL)
        {
            return NULL;
        }

        return converted;
    }

    aml_object_t* converted = NULL;
    if (aml_convert_source(state, object, &converted, targetTypes) == _FAIL)
    {
        return NULL;
    }

    return converted;
}
