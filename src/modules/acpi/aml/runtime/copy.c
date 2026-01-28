#include <kernel/acpi/aml/runtime/copy.h>

#include <kernel/acpi/aml/runtime/buffer_field.h>
#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/acpi/aml/runtime/field_unit.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/log/log.h>

status_t aml_copy_data_and_type(aml_object_t* src, aml_object_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    if (!(src->type & AML_DATA_REF_OBJECTS))
    {
        LOG_ERR("cannot copy object of type '%s'\n", aml_type_to_string(src->type));
        return ERR(ACPI, INVAL);
    }

    status_t status = OK;

    switch (src->type)
    {
    case AML_INTEGER:
        status = aml_integer_set(dest, src->integer.value);
        break;
    case AML_STRING:
        status = aml_string_set(dest, src->string.content);
        break;
    case AML_BUFFER:
        status = aml_buffer_set(dest, src->buffer.content, src->buffer.length, src->buffer.length);
        break;
    case AML_PACKAGE:
        status = aml_package_set(dest, src->package.length);
        if (IS_ERR(status))
        {
            break;
        }

        for (uint64_t i = 0; i < src->package.length; i++)
        {
            status = aml_copy_data_and_type(src->package.elements[i], dest->package.elements[i]);
            if (IS_ERR(status))
            {
                aml_object_clear(dest);
                break;
            }
        }
        break;
    case AML_OBJECT_REFERENCE:
        status = aml_object_reference_set(dest, src->objectReference.target);
        break;
    default:
        LOG_ERR("cannot copy object of type '%s'\n", aml_type_to_string(src->type));
        return ERR(ACPI, INVAL);
    }

    if (IS_OK(status))
    {
        // To make debugging easier we copy the name of the object to if the dest is not already named.
        // The copied name would be overwritten if the dest is named later.
        if (!(dest->flags & AML_OBJECT_NAMED) && (src->flags & AML_OBJECT_NAMED))
        {
            dest->name = src->name;
        }

        // Inherits the `AML_OBJECT_EXCEPTION_ON_USE` flag.
        if (src->flags & AML_OBJECT_EXCEPTION_ON_USE)
        {
            dest->flags |= AML_OBJECT_EXCEPTION_ON_USE;
        }
        else
        {
            dest->flags &= ~AML_OBJECT_EXCEPTION_ON_USE;
        }
    }

    return status;
}

status_t aml_copy_object(aml_state_t* state, aml_object_t* src, aml_object_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    if (src->type == AML_UNINITIALIZED)
    {
        return ERR(ACPI, INVAL);
    }

    if (src == dest)
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
                return ERR(MEM, NOMEM);
            }

            dest->arg.value = newValue; // Transfer ownership
            return aml_copy_data_and_type(src, dest->arg.value);
        }

        if (dest->arg.value->type == AML_OBJECT_REFERENCE)
        {
            return aml_copy_object(state, src, dest->arg.value->objectReference.target);
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
        return aml_field_unit_store(state, &dest->fieldUnit, src);
    }
    if (dest->type == AML_BUFFER_FIELD)
    {
        return aml_buffer_field_store(&dest->bufferField, src);
    }

    if (dest->flags & AML_OBJECT_NAMED)
    {
        return aml_convert_result(state, src, dest);
    }

    if (dest->type == AML_UNINITIALIZED)
    {
        return aml_copy_data_and_type(src, dest);
    }

    LOG_ERR("illegal copy operation from type '%s' to type '%s'\n", aml_type_to_string(src->type),
        aml_type_to_string(dest->type));
    return ERR(ACPI, IMPL);
}
