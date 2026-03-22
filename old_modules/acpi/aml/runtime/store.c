#include <kernel/acpi/aml/runtime/store.h>

#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/acpi/aml/runtime/copy.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/log/log.h>

#include <errno.h>

status_t aml_store(aml_state_t* state, aml_object_t* src, aml_object_t* dest)
{
    if (src == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    if (dest == NULL)
    {
        return OK;
    }

    if (dest->type == AML_ARG)
    {
        if (dest->arg.value == NULL) // Is uninitialized
        {
            aml_object_t* newValue = aml_object_new();
            if (newValue == NULL)
            {
                return ERR(ACPI, NOMEM);
            }

            dest->arg.value = newValue; // Transfer ownership
            return aml_copy_data_and_type(src, dest->arg.value);
        }

        if (dest->arg.value->type == AML_OBJECT_REFERENCE)
        {
            return aml_copy_object(state, src, dest->arg.value->objectReference.target);
        }

        return aml_copy_data_and_type(src, dest->arg.value);
    }

    if (dest->type == AML_LOCAL)
    {
        return aml_copy_data_and_type(src, dest->local.value);
    }

    if (dest->type == AML_FIELD_UNIT || dest->type == AML_BUFFER_FIELD)
    {
        return aml_convert_result(state, src, dest);
    }

    if (dest->flags & AML_OBJECT_NAMED)
    {
        return aml_convert_result(state, src, dest);
    }

    if (dest->type == AML_DEBUG_OBJECT)
    {
        return aml_convert(state, src, dest, AML_DEBUG_OBJECT);
    }

    return aml_copy_data_and_type(src, dest);
}
