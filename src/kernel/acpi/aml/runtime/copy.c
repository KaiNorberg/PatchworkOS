#include "copy.h"

#include "acpi/aml/aml_to_string.h"
#include "buffer_field.h"
#include "convert.h"
#include "field_unit.h"
#include "log/log.h"

#include <errno.h>

uint64_t aml_copy_data_and_type(aml_object_t* src, aml_object_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (!(src->type & AML_DATA_REF_OBJECTS))
    {
        LOG_ERR("cannot copy object of type '%s'\n", aml_type_to_string(src->type));
        errno = EINVAL;
        return ERR;
    }

    switch (src->type)
    {
    case AML_INTEGER:
        if (aml_integer_init(dest, src->integer.value) == ERR)
        {
            return ERR;
        }
        break;
    case AML_STRING:
        if (aml_string_init(dest, src->string.content) == ERR)
        {
            return ERR;
        }
        break;
    case AML_BUFFER:
        if (aml_buffer_init(dest, src->buffer.content, src->buffer.length, src->buffer.length) == ERR)
        {
            return ERR;
        }
        break;
    case AML_PACKAGE:
        if (aml_package_init(dest, src->package.length) == ERR)
        {
            return ERR;
        }

        for (uint64_t i = 0; i < src->package.length; i++)
        {
            if (aml_copy_data_and_type(src->package.elements[i], dest->package.elements[i]) == ERR)
            {
                aml_object_deinit(dest);
                return ERR;
            }
        }
        break;
    case AML_OBJECT_REFERENCE:
        if (aml_object_reference_init(dest, src->objectReference.target) == ERR)
        {
            return ERR;
        }
        break;
    default:
        LOG_ERR("cannot copy object of type '%s'\n", aml_type_to_string(src->type));
        errno = EINVAL;
        return ERR;
    }

    // To make debugging easier we copy the name of the object to if the dest is not already named.
    // The copied name would be overwritten if the dest is named later.
    if (!(dest->flags & AML_OBJECT_NAMED) && (src->flags & AML_OBJECT_NAMED))
    {
        strncpy(dest->name.segment, src->name.segment, AML_NAME_LENGTH);
        dest->name.segment[AML_NAME_LENGTH] = '\0';
    }

    return 0;
}

uint64_t aml_copy_object(aml_object_t* src, aml_object_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_UNINITIALIZED)
    {
        errno = EINVAL;
        return ERR;
    }

    if (src == dest)
    {
        return 0;
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

    if (dest->type == AML_FIELD_UNIT)
    {
        if (aml_field_unit_store(&dest->fieldUnit, src) == ERR)
        {
            return ERR;
        }

        return 0;
    }
    else if (dest->type == AML_BUFFER_FIELD)
    {
        if (aml_buffer_field_store(&dest->bufferField, src) == ERR)
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

    LOG_ERR("illegal copy operation from type '%s' to type '%s'\n", aml_type_to_string(src->type),
        aml_type_to_string(dest->type));
    errno = ENOSYS;
    return ERR;
}
