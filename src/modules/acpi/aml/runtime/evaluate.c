#include <kernel/acpi/aml/encoding/arg.h>
#include <kernel/acpi/aml/runtime/evaluate.h>

#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/acpi/aml/runtime/method.h>
#include <kernel/acpi/aml/state.h>

status_t aml_evaluate(aml_state_t* state, aml_object_t* object, aml_type_t targetTypes, aml_object_t** out)
{
    if (state == NULL || object == NULL || out == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    if (state == NULL)
    {
        aml_state_t tempState;
        status_t status = aml_state_init(&tempState, NULL);
        if (IS_ERR(status))
        {
            return status;
        }

        aml_object_t* result = NULL;
        status = aml_evaluate(&tempState, object, targetTypes, &result);
        aml_state_deinit(&tempState);
        return status;
    }

    if (object->type & targetTypes)
    {
        *out = REF(object);
        return OK;
    }

    if (object->type == AML_METHOD)
    {
        aml_object_t* result = NULL;
        status_t status = aml_method_invoke(state, &object->method, NULL, &result);
        if (IS_ERR(status))
        {
            return status;
        }
        UNREF_DEFER(result);

        aml_object_t* converted = NULL;
        status = aml_convert_source(state, result, &converted, targetTypes);
        if (IS_ERR(status))
        {
            return status;
        }

        *out = converted;
        return OK;
    }

    aml_object_t* converted = NULL;
    status_t status = aml_convert_source(state, object, &converted, targetTypes);
    if (IS_ERR(status))
    {
        return status;
    }

    *out = converted;
    return OK;
}