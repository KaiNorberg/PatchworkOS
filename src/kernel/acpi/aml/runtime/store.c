#include "store.h"

#include <errno.h>

#include "acpi/aml/aml_to_string.h"
#include "convert.h"
#include "copy.h"
#include "log/log.h"

uint64_t aml_store(aml_object_t* src, aml_object_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (dest->type == AML_ARG)
    {
        if (dest->arg.value == NULL) // Is uninitialized
        {
            aml_object_t* newValue = aml_object_new(NULL, AML_OBJECT_NONE);
            if (newValue == NULL)
            {
                return ERR;
            }

            dest->arg.value = newValue; // Transfer ownership
            return aml_copy_data_and_type(src, dest->arg.value);
        }

        if (dest->arg.value->type == AML_OBJECT_REFERENCE)
        {
            return aml_copy_object(src, dest->arg.value->objectReference.target);
        }
        else
        {
            return aml_copy_data_and_type(src, dest->arg.value);
        }
    }

    if (dest->type == AML_LOCAL)
    {
        return aml_copy_data_and_type(src, dest->local.value);
    }

    if (dest->type == AML_FIELD_UNIT || dest->type == AML_BUFFER_FIELD)
    {
        return aml_convert_result(src, dest);
    }

    if (dest->flags & AML_OBJECT_NAMED)
    {
        return aml_convert_result(src, dest);
    }

    if (dest->type == AML_DEBUG_OBJECT)
    {
        return aml_convert(src, dest, AML_DEBUG_OBJECT);
    }

    if (dest->type == AML_UNINITIALIZED)
    {
        return aml_copy_data_and_type(src, dest);
    }

    LOG_ERR("illegal store of object %s with flags '0x%x' to destination object of type '%s' with flags '0x%x'\n",
        aml_object_to_string(src), src->flags, aml_type_to_string(dest->type), dest->flags);
    errno = EINVAL;
    return ERR;
}
