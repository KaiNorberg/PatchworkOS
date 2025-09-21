#include "opregion.h"

#include "access_type.h"
#include "acpi/aml/aml_to_string.h"
#include "cpu/port.h"
#include "log/log.h"
#include "mem/vmm.h"

#include <errno.h>

typedef struct aml_region_handler
{
    uint64_t (*read)(uint64_t address, aml_bit_size_t accessSize, uint64_t* out);
    uint64_t (*write)(uint64_t address, aml_bit_size_t accessSize, uint64_t value);
} aml_region_handler_t;

static void* aml_ensure_mem_is_mapped(uint64_t address, aml_bit_size_t accessSize)
{
    size_t accessBytes = (accessSize + 7) / 8;

    bool crossesBoundary = ((uintptr_t)address & (PAGE_SIZE - 1)) + accessBytes > PAGE_SIZE;

    for (uint64_t page = 0; page < (crossesBoundary ? 2 : 1); page++)
    {
        void* pageAddr = (void*)ROUND_DOWN((uintptr_t)address + page * PAGE_SIZE, PAGE_SIZE);
        void* virtAddr = vmm_kernel_map(NULL, pageAddr, 1, PML_WRITE);
        if (virtAddr == NULL && errno != EEXIST) // EEXIST means already mapped
        {
            LOG_ERR("failed to map physical address %p for opregion access\n", pageAddr);
            errno = EIO;
            return NULL;
        }
    }

    return PML_LOWER_TO_HIGHER((void*)address);
}

static uint64_t aml_system_mem_read(uint64_t address, aml_bit_size_t accessSize, uint64_t* out)
{
    void* physAddr = (void*)address;
    void* virtAddr = aml_ensure_mem_is_mapped(address, accessSize);
    if (virtAddr == NULL)
    {
        return ERR;
    }

    switch (accessSize)
    {
    case 8:
        *out = *(volatile uint8_t*)virtAddr;
        break;
    case 16:
        *out = *(volatile uint16_t*)virtAddr;
        break;
    case 32:
        *out = *(volatile uint32_t*)virtAddr;
        break;
    case 64:
        *out = *(volatile uint64_t*)virtAddr;
        break;
    default:
        LOG_ERR("invalid opregion read with access size %u\n", accessSize);
        errno = ENOSYS;
        return ERR;
    }
    return 0;
}

static uint64_t aml_system_mem_write(uint64_t address, aml_bit_size_t accessSize, uint64_t value)
{
    void* physAddr = (void*)address;
    void* virtAddr = aml_ensure_mem_is_mapped(address, accessSize);
    if (virtAddr == NULL)
    {
        return ERR;
    }

    switch (accessSize)
    {
    case 8:
        *(volatile uint8_t*)virtAddr = (uint8_t)value;
        break;
    case 16:
        *(volatile uint16_t*)virtAddr = (uint16_t)value;
        break;
    case 32:
        *(volatile uint32_t*)virtAddr = (uint32_t)value;
        break;
    case 64:
        *(volatile uint64_t*)virtAddr = value;
        break;
    default:
        LOG_ERR("invalid opregion write with access size %u\n", accessSize);
        errno = ENOSYS;
        return ERR;
    }
    return 0;
}

static uint64_t aml_system_io_read(uint64_t address, aml_bit_size_t accessSize, uint64_t* out)
{
    switch (accessSize)
    {
    case 8:
        *out = port_inb(address);
        break;
    case 16:
        *out = port_inw(address);
        break;
    case 32:
        *out = port_inl(address);
        break;
    default:
        LOG_ERR("unable to read opregion with access size %u\n", accessSize);
        errno = ENOSYS;
        return ERR;
    }
    return 0;
}

static uint64_t aml_system_io_write(uint64_t address, aml_bit_size_t accessSize, uint64_t value)
{
    switch (accessSize)
    {
    case 8:
        port_outb(address, (uint8_t)value);
        break;
    case 16:
        port_outw(address, (uint16_t)value);
        break;
    case 32:
        port_outl(address, (uint32_t)value);
        break;
    default:
        LOG_ERR("unable to write opregion with access size %u\n", accessSize);
        errno = ENOSYS;
        return ERR;
    }
    return 0;
}

static aml_region_handler_t regionHandlers[] = {
    [AML_REGION_SYSTEM_MEMORY] = {.read = aml_system_mem_read, .write = aml_system_mem_write},
    [AML_REGION_SYSTEM_IO] = {.read = aml_system_io_read, .write = aml_system_io_write},
};

#define AML_REGION_MAX (sizeof(regionHandlers) / sizeof(regionHandlers[0]))

static inline uint64_t aml_opregion_read(aml_region_space_t space, uint64_t address, aml_bit_size_t accessSize,
    uint64_t* out)
{
    if (out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    if (space >= AML_REGION_MAX || regionHandlers[space].read == NULL)
    {
        LOG_ERR("unimplemented opregion read with opregion space '%s'\n", aml_region_space_to_string(space));
        errno = ENOSYS;
        return ERR;
    }
    return regionHandlers[space].read(address, accessSize, out);
}

static inline uint64_t aml_opregion_write(aml_region_space_t space, uint64_t address, aml_bit_size_t accessSize,
    uint64_t value)
{
    if (space >= AML_REGION_MAX || regionHandlers[space].write == NULL)
    {
        LOG_ERR("unimplemented opregion write with opregion space '%s'\n", aml_region_space_to_string(space));
        errno = ENOSYS;
        return ERR;
    }
    return regionHandlers[space].write(address, accessSize, value);
}

static inline uint64_t aml_get_aligned_byte_offset(aml_bit_size_t bitOffset, aml_bit_size_t accessSize)
{
    // Bit manipulation magic to find the byte offset and align it to the access size.
    return (bitOffset & ~(accessSize - 1)) / 8;
}

typedef enum aml_access_direction
{
    AML_ACCESS_READ,
    AML_ACCESS_WRITE
} aml_access_direction_t;

typedef struct
{
    aml_node_type_t type;
    aml_region_space_t space;
    aml_bit_size_t bitSize;
    aml_bit_size_t bitOffset;
    uintptr_t base;
    aml_field_flags_t flags;
    aml_node_t* indexNode; // Only for IndexFields
    aml_node_t* dataNode;  // Only for IndexFields
} aml_field_access_data_t;

static uint64_t aml_extract_field_access_data(aml_node_t* field, aml_field_access_data_t* out)
{
    out->type = field->type;

    switch (field->type)
    {
    case AML_NODE_FIELD:
        out->space = field->field.opregion->opregion.space;
        out->bitSize = field->field.bitSize;
        out->bitOffset = field->field.bitOffset;
        out->base = field->field.opregion->opregion.offset;
        out->flags = field->field.flags;
        out->indexNode = NULL;
        out->dataNode = NULL;
        return 0;
    case AML_NODE_INDEX_FIELD:
        // The space is the space of the IndexFields data node's opregion.
        out->space = field->indexField.dataNode->field.opregion->opregion.space;
        out->bitSize = field->indexField.bitSize;
        out->bitOffset = field->indexField.bitOffset;
        out->base = field->indexField.dataNode->field.opregion->opregion.offset;
        out->flags = field->indexField.flags;
        out->indexNode = field->indexField.indexNode;
        out->dataNode = field->indexField.dataNode;
        return 0;
    default:
        LOG_ERR("invalid field node type %s\n", aml_node_type_to_string(field->type));
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_generic_field_read_at(aml_field_access_data_t* accessData, uint64_t address,
    aml_bit_size_t accessSize, aml_bit_size_t inAccessOffset, aml_bit_size_t bitsToAccess, uint64_t byteOffset,
    uint64_t* out)
{
    switch (accessData->type)
    {
    case AML_NODE_FIELD:
        return aml_opregion_read(accessData->space, address, accessSize, out);
    case AML_NODE_INDEX_FIELD:
    {
        aml_data_object_t temp;
        if (aml_data_object_init_integer(&temp, byteOffset, 64) == ERR)
        {
            return ERR;
        }

        if (aml_field_write(accessData->indexNode, &temp) == ERR)
        {
            aml_data_object_deinit(&temp);
            return ERR;
        }
        aml_data_object_deinit(&temp);

        if (aml_field_read(accessData->dataNode, &temp) == ERR)
        {
            return ERR;
        }

        if (temp.type != AML_DATA_INTEGER)
        {
            LOG_ERR("IndexField data node '%.*s' did not return an integer\n", AML_NAME_LENGTH,
                accessData->dataNode->segment);
            aml_data_object_deinit(&temp);
            errno = EILSEQ;
            return ERR;
        }

        *out = temp.integer;
        aml_data_object_deinit(&temp);
        return 0;
    }
    default:
        LOG_ERR("invalid field node type %s\n", aml_node_type_to_string(accessData->type));
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_generic_field_write_at(aml_field_access_data_t* accessData, uint64_t address,
    aml_bit_size_t accessSize, aml_bit_size_t inAccessOffset, aml_bit_size_t bitsToAccess, uint64_t byteOffset,
    uint64_t value)
{
    switch (accessData->type)
    {
    case AML_NODE_FIELD:
        return aml_opregion_write(accessData->space, address, accessSize, value);
    case AML_NODE_INDEX_FIELD:
    {
        aml_data_object_t indexObj;
        if (aml_data_object_init_integer(&indexObj, byteOffset, 64) == ERR)
        {
            return ERR;
        }

        if (aml_field_write(accessData->indexNode, &indexObj) == ERR)
        {
            aml_data_object_deinit(&indexObj);
            return ERR;
        }
        aml_data_object_deinit(&indexObj);

        aml_data_object_t dataObj;
        if (aml_data_object_init_integer(&dataObj, value, 64) == ERR)
        {
            return ERR;
        }

        if (aml_field_write(accessData->dataNode, &dataObj) == ERR)
        {
            aml_data_object_deinit(&dataObj);
            return ERR;
        }
        aml_data_object_deinit(&dataObj);
        return 0;
    }
    default:
        LOG_ERR("invalid field node type %s\n", aml_node_type_to_string(accessData->type));
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_generic_field_access(aml_node_t* field, aml_data_object_t* data, aml_access_direction_t direction)
{
    aml_field_access_data_t accessData;
    if (aml_extract_field_access_data(field, &accessData) == ERR)
    {
        return ERR;
    }

    aml_bit_size_t accessSize;
    if (aml_get_access_size(accessData.bitSize, accessData.flags.accessType, accessData.space, &accessSize) == ERR)
    {
        return ERR;
    }

    LOG_DEBUG("%s field '%.*s' of size %u bits with access size %u bits from opregion space '%s'\n",
        direction == AML_ACCESS_READ ? "reading" : "writing to", AML_NAME_LENGTH, field->segment, accessData.bitSize,
        accessSize, aml_region_space_to_string(accessData.space));

    uint64_t byteOffset = aml_get_aligned_byte_offset(accessData.bitOffset, accessSize);

    uint64_t currentPos = 0;
    while (currentPos < accessData.bitSize)
    {
        aml_bit_size_t inAccessOffset = (accessData.bitOffset + currentPos) & (accessSize - 1);
        aml_bit_size_t bitsToAccess = MIN(accessData.bitSize - currentPos, accessSize - inAccessOffset);
        uint64_t mask = (UINT64_C(1) << bitsToAccess) - 1;

        uint64_t address = accessData.base + byteOffset;

        switch (direction)
        {
        case AML_ACCESS_READ:
        {
            uint64_t value;
            if (aml_generic_field_read_at(&accessData, address, accessSize, inAccessOffset, bitsToAccess, byteOffset,
                    &value) == ERR)
            {
                return ERR;
            }

            value = (value >> inAccessOffset) & mask;

            if (aml_data_object_put_bits_at(data, value, currentPos, bitsToAccess) == ERR)
            {
                return ERR;
            }
        }
        break;
        case AML_ACCESS_WRITE:
        {
            uint64_t value;
            switch (accessData.flags.updateRule)
            {
            case AML_UPDATE_RULE_PRESERVE:
            {
                if (aml_generic_field_read_at(&accessData, address, accessSize, inAccessOffset, accessSize, byteOffset,
                        &value) == ERR)
                {
                    return ERR;
                }
            }
            break;
            case AML_UPDATE_RULE_WRITE_AS_ONES:
                value = UINT64_C(-1);
                break;
            case AML_UPDATE_RULE_WRITE_AS_ZEROS:
                value = 0;
                break;
            default:
                LOG_ERR("invalid field update rule %d\n", accessData.flags.updateRule);
                errno = EINVAL;
                return ERR;
            }

            // Clear the bits we are going to write to, then set them to the new value.
            value &= ~mask;

            uint64_t newValue;
            if (aml_data_object_get_bits_at(data, currentPos, bitsToAccess, &newValue) == ERR)
            {
                return ERR;
            }
            value |= (newValue << inAccessOffset) & mask;

            if (aml_generic_field_write_at(&accessData, address, accessSize, inAccessOffset, bitsToAccess, byteOffset,
                    value) == ERR)
            {
                return ERR;
            }
        }
        break;
        default:
            LOG_ERR("invalid field access direction %d\n", direction);
            errno = EINVAL;
            return ERR;
        }

        currentPos += bitsToAccess;
        byteOffset += accessSize / 8;
    }

    return 0;
}

uint64_t aml_field_read(aml_node_t* field, aml_data_object_t* out)
{
    if (field == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t byteSize = (field->field.bitSize + 7) / 8;
    if (byteSize > sizeof(aml_qword_data_t))
    {
        if (aml_data_object_init_buffer_empty(out, byteSize) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_data_object_init_integer(out, 0, byteSize * 8) == ERR)
        {
            return ERR;
        }
    }

    uint64_t result = aml_generic_field_access(field, out, AML_ACCESS_READ);
    if (result == ERR)
    {
        aml_data_object_deinit(out);
    }
    return result;
}

uint64_t aml_field_write(aml_node_t* field, aml_data_object_t* in)
{
    if (field == NULL || in == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return aml_generic_field_access(field, in, AML_ACCESS_WRITE);
}

uint64_t aml_index_field_read(aml_node_t* indexField, aml_data_object_t* out)
{
    if (indexField == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t byteSize = (indexField->indexField.bitSize + 7) / 8;
    if (byteSize > sizeof(aml_qword_data_t))
    {
        if (aml_data_object_init_buffer_empty(out, byteSize) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_data_object_init_integer(out, 0, byteSize * 8) == ERR)
        {
            return ERR;
        }
    }

    uint64_t result = aml_generic_field_access(indexField, out, AML_ACCESS_READ);
    if (result == ERR)
    {
        aml_data_object_deinit(out);
    }
    return result;
}

uint64_t aml_index_field_write(aml_node_t* indexField, aml_data_object_t* in)
{
    if (indexField == NULL || in == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    return aml_generic_field_access(indexField, in, AML_ACCESS_WRITE);
}
