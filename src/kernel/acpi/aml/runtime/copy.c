#include "copy.h"

#include "convert.h"
#include "log/log.h"
#include "acpi/aml/aml_to_string.h"

#include <errno.h>

uint64_t aml_copy_raw(aml_node_t* src, aml_node_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (src->type)
    {
    case AML_DATA_BUFFER:
        if (aml_node_init_buffer(dest, src->buffer.content, src->buffer.length, src->buffer.length) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_BUFFER_FIELD:
        if (aml_node_init_buffer_field(dest, src->bufferField.buffer, src->bufferField.bitOffset, src->bufferField.bitSize) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_DEVICE:
        if (aml_node_init_device(dest) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_FIELD_UNIT:
        switch (src->fieldUnit.type)
        {
        case AML_FIELD_UNIT_FIELD:
            if (aml_node_init_field_unit_field(dest, src->fieldUnit.opregion, src->fieldUnit.flags,
                    src->fieldUnit.bitOffset, src->fieldUnit.bitSize) == ERR)
            {
                return ERR;
            }
            break;
        case AML_FIELD_UNIT_INDEX_FIELD:
            if (aml_node_init_field_unit_index_field(dest, src->fieldUnit.indexNode, src->fieldUnit.dataNode,
                    src->fieldUnit.flags, src->fieldUnit.bitOffset, src->fieldUnit.bitSize) == ERR)
            {
                return ERR;
            }
            break;
        case AML_FIELD_UNIT_BANK_FIELD:
            if (aml_node_init_field_unit_bank_field(dest, src->fieldUnit.opregion, src->fieldUnit.bank,
                    src->fieldUnit.bankValue, src->fieldUnit.flags, src->fieldUnit.bitOffset, src->fieldUnit.bitSize) == ERR)
            {
                return ERR;
            }
            break;
        default:
            errno = EINVAL;
            return ERR;
        }
        break;
    case AML_DATA_INTEGER:
        if (aml_node_init_integer(dest, src->integer.value) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_INTEGER_CONSTANT:
        if (aml_node_init_integer_constant(dest, src->integerConstant.value) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_METHOD:
        if (aml_node_init_method(dest, &src->method.flags, src->method.start, src->method.end, src->method.implementation) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_MUTEX:
        if (aml_node_init_mutex(dest, src->mutex.syncLevel) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_OBJECT_REFERENCE:
        if (aml_node_init_object_reference(dest, src->objectReference.target) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_OPERATION_REGION:
        if (aml_node_init_operation_region(dest, src->opregion.space, src->opregion.offset, src->opregion.length) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_PACKAGE:
        if (aml_node_init_package(dest, src->package.length) == ERR)
        {
            return ERR;
        }

        for (uint64_t i = 0; i < src->package.length; i++)
        {
            if (aml_copy_raw(src->package.elements[i], dest->package.elements[i]) == ERR)
            {
                for (uint64_t j = 0; j < i; j++)
                {
                    aml_node_deinit(dest->package.elements[j]);
                }
                aml_node_deinit(dest);
                return ERR;
            }
        }
        break;
    case AML_DATA_PROCESSOR:
        if (aml_node_init_processor(dest, src->processor.procId, src->processor.pblkAddr, src->processor.pblkLen) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_STRING:
        if (aml_node_init_string(dest, src->string.content) == ERR)
        {
            return ERR;
        }
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

uint64_t aml_copy(aml_node_t* src, aml_node_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_DATA_UNINITALIZED)
    {
        errno = EINVAL;
        return ERR;
    }

    if (src == dest)
    {
        return 0;
    }

    // If of type
    // "Method ArgX variable"
    // then
    // "The object is copied to the destination
    // with no conversion applied, with one ex-
    // ception. If the ArgX contains an Object
    // Reference, an automatic de-reference
    // occurs and the object is copied to the
    // target of the Object Reference instead of
    // overwriting the contents of ArgX."
    if (dest->flags & AML_NODE_ARG)
    {
        if (dest->type == AML_DATA_OBJECT_REFERENCE)
        {
            aml_node_t* target = dest->objectReference.target;
            if (target == NULL)
            {
                errno = EINVAL;
                return ERR;
            }

            if (aml_copy_raw(src, target) == ERR)
            {
                return ERR;
            }
        }
        else
        {
            if (aml_copy_raw(src, dest) == ERR)
            {
                return ERR;
            }
        }

        return 0;
    }

    // If of type
    // "Method LocalX variable"
    // then
    // "The object is copied to the destination
    // with no conversion applied. Even if Lo-
    // calX contains an Object Reference, it is
    // overwritten."
    if (dest->flags & AML_NODE_LOCAL)
    {
        if (aml_copy_raw(src, dest) == ERR)
        {
            return ERR;
        }

        return 0;
    }

    // If of type
    // "Field Unit or BufferField"
    // then
    // "overwritten.
    // The object is copied to the destination
    // after implicit result conversion is ap-
    // plied."
    if (dest->type == AML_DATA_FIELD_UNIT || dest->type == AML_DATA_BUFFER_FIELD)
    {
        if (aml_convert_result(src, dest) == ERR)
        {
            return ERR;
        }

        return 0;
    }

    // If of type
    // "Named data object"
    // then
    // "The object is copied to the destination
    // after implicit result conversion is ap-
    // plied to match the existing type of the
    // named location."
    if (dest->flags & AML_NODE_NAMED)
    {
        if (aml_convert_result(src, dest) == ERR)
        {
            return ERR;
        }

        return 0;
    }

    LOG_ERR("illegal copy operation from type '%s' to type '%s'\n", aml_data_type_to_string(src->type), aml_data_type_to_string(dest->type));
    errno = ENOSYS;
    return ERR;
}
