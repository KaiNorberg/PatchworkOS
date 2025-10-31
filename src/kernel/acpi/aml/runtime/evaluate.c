#include <kernel/acpi/aml/runtime/evaluate.h>

#include <kernel/acpi/aml/runtime/method.h>
#include <kernel/acpi/aml/runtime/convert.h>

aml_object_t* aml_evaluate(aml_state_t* state, aml_object_t* object, aml_type_t targetTypes)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return NULL;
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
        DEREF_DEFER(result);

        aml_object_t* converted;
        if (aml_convert_source(state, result, &converted, targetTypes) == ERR)
        {
            return NULL;
        }

        return converted;
    }

    aml_object_t* converted;
    if (aml_convert_source(state, object, &converted, targetTypes) == ERR)
    {
        return NULL;
    }

    return converted;
}
