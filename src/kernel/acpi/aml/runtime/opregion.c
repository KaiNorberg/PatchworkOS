#include "opregion.h"

#include "access_type.h"
#include "acpi/aml/aml_to_string.h"
#include "log/log.h"

#include <errno.h>

uint64_t aml_field_read(aml_node_t* field, aml_data_object_t* out)
{
    if (field == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    // Retrieve field properties.
    aml_bit_size_t bitSize;
    aml_bit_size_t bitOffset;
    aml_access_type_t accessType;
    aml_region_space_t regionSpace;
    switch (field->type)
    {
    case AML_NODE_FIELD:
        bitSize = field->field.bitSize;
        bitOffset = field->field.bitOffset;
        accessType = field->field.flags.accessType;
        regionSpace = field->field.opregion->opregion.space;
        break;
    case AML_NODE_INDEX_FIELD:
        bitSize = field->indexField.bitSize;
        bitOffset = field->indexField.bitOffset;
        accessType = field->indexField.flags.accessType;
        regionSpace = field->indexField.indexNode->field.opregion->opregion.space;
        break;
    case AML_NODE_BANK_FIELD:
        LOG_ERR("BankField read not implemented\n");
        errno = ENOSYS;
        return ERR;
    default:
        LOG_ERR("attempted to read from non-field node '%.*s' of type '%s'\n", AML_NAME_LENGTH, field->name,
            aml_node_type_to_string(field->type));
        errno = EILSEQ;
        return ERR;
    }

    aml_bit_size_t accessSize;
    if (aml_get_access_size(bitSize, accessType, regionSpace, &accessSize) == ERR)
    {
        return ERR;
    }

    uint64_t byteSize = (bitSize + 7) / 8;

    // Bit manipulation magic to find the byte offset and align it to the access size.
    uint64_t byteOffset = (bitOffset & ~(accessSize - 1)) / 8;

    if (byteSize > sizeof(aml_qword_data_t))
    {
        if (aml_data_object_init_buffer_empty(out, byteSize) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        // Use byteSize * 8 and not bitSize to ensure its rounded.
        if (aml_data_object_init_integer(out, 0, byteSize * 8) == ERR)
        {
            return ERR;
        }
    }

    uint64_t currentPos = 0;
    while (currentPos < bitSize)
    {
        aml_bit_size_t inAccessOffset = (bitOffset + currentPos) & (accessSize - 1);
        aml_bit_size_t bitsToAccess = MIN(bitSize - currentPos, accessSize - inAccessOffset);
        uint64_t mask = (UINT64_C(1) << bitsToAccess) - 1;

        uint64_t value;
        switch (field->type)
        {
        case AML_NODE_FIELD:
            LOG_ERR("Field read not implemented\n");
            errno = ENOSYS;
            return ERR;
        case AML_NODE_INDEX_FIELD:
            LOG_ERR("IndexField read not implemented\n");
            errno = ENOSYS;
            return ERR;
        case AML_NODE_BANK_FIELD:
            LOG_ERR("BankField read not implemented\n");
            errno = ENOSYS;
            return ERR;
        default:
            LOG_ERR("attempted to read from non-field node '%.*s' of type '%s'\n", AML_NAME_LENGTH, field->name,
                aml_node_type_to_string(field->type));
            errno = EILSEQ;
            return ERR;
        }

        value = (value >> inAccessOffset) & mask;

        if (aml_data_object_put_bits_at(out, value, currentPos, bitsToAccess) == ERR)
        {
            return ERR;
        }

        currentPos += bitsToAccess;
        byteOffset += accessSize / 8;
    }

    return 0;
}
