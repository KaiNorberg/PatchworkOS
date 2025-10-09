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

    if (dest->flags & AML_OBJECT_ARG)
    {
        if (dest->type == AML_OBJECT_REFERENCE)
        {
            aml_object_t* target = dest->objectReference.target;
            if (target == NULL)
            {
                errno = EINVAL;
                return ERR;
            }

            if (aml_copy_data_and_type(src, target) == ERR)
            {
                return ERR;
            }
        }
        else
        {
            if (aml_copy_data_and_type(src, dest) == ERR)
            {
                return ERR;
            }
        }

        return 0;
    }

    if (dest->flags & AML_OBJECT_LOCAL)
    {
        if (aml_copy_data_and_type(src, dest) == ERR)
        {
            return ERR;
        }

        return 0;
    }

    if (dest->type == AML_FIELD_UNIT || dest->type == AML_BUFFER_FIELD)
    {
        if (aml_convert_result(src, dest) == ERR)
        {
            return ERR;
        }

        return 0;
    }

    if (dest->flags & AML_OBJECT_NAMED)
    {
        if (aml_convert_result(src, dest) == ERR)
        {
            return ERR;
        }

        return 0;
    }

    if (dest->type == AML_DEBUG_OBJECT)
    {
        if (aml_convert(src, dest, AML_DEBUG_OBJECT) == ERR)
        {
            return ERR;
        }
        return 0;
    }

    LOG_ERR("invalid destination object of type '%s' with flags '0x%x'\n", aml_type_to_string(dest->type), dest->flags);
    errno = EINVAL;
    return ERR;
}
