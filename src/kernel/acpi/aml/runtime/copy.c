#include "copy.h"

#include "acpi/aml/to_string.h"
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
        if (aml_integer_set(dest, src->integer.value) == ERR)
        {
            return ERR;
        }
        break;
    case AML_STRING:
        if (aml_string_set(dest, src->string.content) == ERR)
        {
            return ERR;
        }
        break;
    case AML_BUFFER:
        if (aml_buffer_set(dest, src->buffer.content, src->buffer.length, src->buffer.length) == ERR)
        {
            return ERR;
        }
        break;
    case AML_PACKAGE:
        if (aml_package_set(dest, src->package.length) == ERR)
        {
            return ERR;
        }

        for (uint64_t i = 0; i < src->package.length; i++)
        {
            if (aml_copy_data_and_type(src->package.elements[i], dest->package.elements[i]) == ERR)
            {
                aml_object_clear(dest);
                return ERR;
            }
        }
        break;
    case AML_OBJECT_REFERENCE:
        if (aml_object_reference_set(dest, src->objectReference.target) == ERR)
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

    // TODO: Windows seems to allow this so we do the same but I need to do more reading on the whole Windows acpi
    // weirdness thing.
    if (src == dest)
    {
        return 0;
    }

    if (dest->type == AML_ARG)
    {
        if (dest->arg.value == NULL) // Is uninitialized
        {
            aml_object_t* newValue = aml_object_new(NULL);
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

    if (dest->type == AML_FIELD_UNIT)
    {
        return aml_field_unit_store(&dest->fieldUnit, src);
    }
    if (dest->type == AML_BUFFER_FIELD)
    {
        return aml_buffer_field_store(&dest->bufferField, src);
    }

    if (dest->flags & AML_OBJECT_NAMED)
    {
        return aml_convert_result(src, dest);
    }

    if (dest->type == AML_UNINITIALIZED)
    {
        return aml_copy_data_and_type(src, dest);
    }

    LOG_ERR("illegal copy operation from type '%s' to type '%s'\n", aml_type_to_string(src->type),
        aml_type_to_string(dest->type));
    errno = ENOSYS;
    return ERR;
}
