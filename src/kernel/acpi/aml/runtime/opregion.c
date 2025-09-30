#include "opregion.h"

#include "access_type.h"
#include "acpi/aml/aml_to_string.h"
#include "cpu/port.h"
#include "drivers/pci/pci_config.h"
#include "log/log.h"
#include "mem/vmm.h"
#include "method.h"

#include <errno.h>

typedef struct aml_region_handler
{
    uint64_t (*read)(aml_node_t* opregion, uint64_t address, aml_bit_size_t accessSize, uint64_t* out);
    uint64_t (*write)(aml_node_t* opregion, uint64_t address, aml_bit_size_t accessSize, uint64_t value);
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

static uint64_t aml_system_mem_read(aml_node_t* opregion, uint64_t address, aml_bit_size_t accessSize, uint64_t* out)
{
    (void)opregion;

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

static uint64_t aml_system_mem_write(aml_node_t* opregion, uint64_t address, aml_bit_size_t accessSize, uint64_t value)
{
    (void)opregion;

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

static uint64_t aml_system_io_read(aml_node_t* opregion, uint64_t address, aml_bit_size_t accessSize, uint64_t* out)
{
    (void)opregion;

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

static uint64_t aml_system_io_write(aml_node_t* opregion, uint64_t address, aml_bit_size_t accessSize, uint64_t value)
{
    (void)opregion;

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

static uint64_t aml_pci_get_params(aml_node_t* opregion, pci_segment_group_t* segmentGroup, pci_bus_t* bus,
    pci_slot_t* slot, pci_function_t* function)
{
    // Note that the aml_node_find function will recursively search parent scopes.

    // We assume zero for all parameters if the corresponding node is not found.
    *segmentGroup = 0;
    *bus = 0;
    *slot = 0;
    *function = 0;

    // See section 6.1.1 of the ACPI specification.
    aml_node_t* adrNode = aml_node_find(opregion, "_ADR");
    if (adrNode != NULL)
    {
        uint64_t adrValue = 0;
        if (aml_method_evaluate_integer(adrNode, &adrValue) == ERR)
        {
            LOG_ERR("failed to evaluate _ADR for opregion '%.*s'\n", AML_NAME_LENGTH, opregion->segment);
            return ERR;
        }

        *slot = adrValue & 0x0000FFFF;             // Low word is slot
        *function = (adrValue >> 16) & 0x0000FFFF; // High word is function
    }

    // Section 6.5.5 of the ACPI specification.
    aml_node_t* bbnNode = aml_node_find(opregion, "_BBN");
    if (bbnNode != NULL)
    {
        uint64_t bbnValue = 0;
        if (aml_method_evaluate_integer(bbnNode, &bbnValue) == ERR)
        {
            LOG_ERR("failed to evaluate _BBN for opregion '%.*s'\n", AML_NAME_LENGTH, opregion->segment);
            return ERR;
        }

        // Lower 8 bits is the bus number.
        *bus = bbnValue & 0xFF;
    }

    // Section 6.5.6 of the ACPI specification.
    aml_node_t* segNode = aml_node_find(opregion, "_SEG");
    if (segNode != NULL)
    {
        uint64_t segValue = 0;
        if (aml_method_evaluate_integer(segNode, &segValue) == ERR)
        {
            LOG_ERR("failed to evaluate _SEG for opregion '%.*s'\n", AML_NAME_LENGTH, opregion->segment);
            return ERR;
        }

        // Lower 16 bits is the segment group number.
        *segmentGroup = segValue & 0xFFFF;
    }

    return 0;
}

static uint64_t aml_pci_config_read(aml_node_t* opregion, uint64_t address, aml_bit_size_t accessSize, uint64_t* out)
{
    pci_segment_group_t segmentGroup;
    pci_bus_t bus;
    pci_slot_t slot;
    pci_function_t function;
    if (aml_pci_get_params(opregion, &segmentGroup, &bus, &slot, &function) == ERR)
    {
        return ERR;
    }

    switch (accessSize)
    {
    case 8:
        *out = pci_config_read8(segmentGroup, bus, slot, function, (uint16_t)address);
        break;
    case 16:
        *out = pci_config_read16(segmentGroup, bus, slot, function, (uint16_t)address);
        break;
    case 32:
        *out = pci_config_read32(segmentGroup, bus, slot, function, (uint16_t)address);
        break;
    default:
        LOG_ERR("unable to read PCI config opregion with access size %u\n", accessSize);
        errno = ENOSYS;
        return ERR;
    }
    return 0;
}

static uint64_t aml_pci_config_write(aml_node_t* opregion, uint64_t address, aml_bit_size_t accessSize, uint64_t value)
{
    pci_segment_group_t segmentGroup;
    pci_bus_t bus;
    pci_slot_t slot;
    pci_function_t function;
    if (aml_pci_get_params(opregion, &segmentGroup, &bus, &slot, &function) == ERR)
    {
        return ERR;
    }

    switch (accessSize)
    {
    case 8:
        pci_config_write8(segmentGroup, bus, slot, function, (uint16_t)address, (uint8_t)value);
        break;
    case 16:
        pci_config_write16(segmentGroup, bus, slot, function, (uint16_t)address, (uint16_t)value);
        break;
    case 32:
        pci_config_write32(segmentGroup, bus, slot, function, (uint16_t)address, (uint32_t)value);
        break;
    default:
        LOG_ERR("unable to write PCI config opregion with access size %u\n", accessSize);
        errno = ENOSYS;
        return ERR;
    }
    return 0;
}

static aml_region_handler_t regionHandlers[] = {
    [AML_REGION_SYSTEM_MEMORY] = {.read = aml_system_mem_read, .write = aml_system_mem_write},
    [AML_REGION_SYSTEM_IO] = {.read = aml_system_io_read, .write = aml_system_io_write},
    [AML_REGION_PCI_CONFIG] = {.read = aml_pci_config_read, .write = aml_pci_config_write},
};

#define AML_REGION_MAX (sizeof(regionHandlers) / sizeof(regionHandlers[0]))

static inline uint64_t aml_opregion_read(aml_node_t* opregion, aml_region_space_t space, uint64_t address,
    aml_bit_size_t accessSize, uint64_t* out)
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
    return regionHandlers[space].read(opregion, address, accessSize, out);
}

static inline uint64_t aml_opregion_write(aml_node_t* opregion, aml_region_space_t space, uint64_t address,
    aml_bit_size_t accessSize, uint64_t value)
{
    if (space >= AML_REGION_MAX || regionHandlers[space].write == NULL)
    {
        LOG_ERR("unimplemented opregion write with opregion space '%s'\n", aml_region_space_to_string(space));
        errno = ENOSYS;
        return ERR;
    }
    return regionHandlers[space].write(opregion, address, accessSize, value);
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

static uint64_t aml_generic_field_read_at(aml_node_t* fieldUnit, aml_bit_size_t accessSize, uint64_t byteOffset,
    uint64_t* out)
{
    switch (fieldUnit->fieldUnit.type)
    {
    case AML_FIELD_UNIT_FIELD:
    case AML_FIELD_UNIT_BANK_FIELD:
    {
        aml_node_t* opregion = fieldUnit->fieldUnit.opregion;
        uintptr_t address = opregion->opregion.offset + byteOffset;
        return aml_opregion_read(opregion, fieldUnit->fieldUnit.regionSpace, address, accessSize, out);
    }
    case AML_FIELD_UNIT_INDEX_FIELD:
    {
        aml_node_t temp = AML_NODE_CREATE(AML_NODE_NONE);
        if (aml_node_init_integer(&temp, byteOffset) == ERR)
        {
            return ERR;
        }

        if (aml_field_unit_store(fieldUnit->fieldUnit.indexNode, &temp) == ERR)
        {
            aml_node_deinit(&temp);
            return ERR;
        }
        aml_node_deinit(&temp);

        if (aml_field_unit_load(fieldUnit->fieldUnit.dataNode, &temp) == ERR)
        {
            return ERR;
        }

        // A field cant return anything larger then 64 bits so it has to be an integer.
        if (temp.type != AML_DATA_INTEGER)
        {
            LOG_ERR("IndexField data node '%.*s' did not return an integer\n", AML_NAME_LENGTH,
                fieldUnit->fieldUnit.dataNode->segment);
            aml_node_deinit(&temp);
            errno = EILSEQ;
            return ERR;
        }

        *out = temp.integer.value;
        aml_node_deinit(&temp);
        return 0;
    }
    default:
        LOG_ERR("invalid field node type %d\n", fieldUnit->fieldUnit.type);
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_generic_field_write_at(aml_node_t* fieldUnit, aml_bit_size_t accessSize, uint64_t byteOffset,
    uint64_t value)
{
    switch (fieldUnit->fieldUnit.type)
    {
    case AML_FIELD_UNIT_FIELD:
    case AML_FIELD_UNIT_BANK_FIELD:
    {
        aml_node_t* opregion = fieldUnit->fieldUnit.opregion;
        uintptr_t address = opregion->opregion.offset + byteOffset;
        return aml_opregion_write(opregion, fieldUnit->fieldUnit.regionSpace, address, accessSize, value);
    }
    case AML_FIELD_UNIT_INDEX_FIELD:
    {
        aml_node_t index = AML_NODE_CREATE(AML_NODE_NONE);
        if (aml_node_init_integer(&index, byteOffset) == ERR)
        {
            return ERR;
        }

        if (aml_field_unit_store(fieldUnit->fieldUnit.indexNode, &index) == ERR)
        {
            aml_node_deinit(&index);
            return ERR;
        }
        aml_node_deinit(&index);

        aml_node_t data = AML_NODE_CREATE(AML_NODE_NONE);
        if (aml_node_init_integer(&data, value) == ERR)
        {
            return ERR;
        }

        if (aml_field_unit_store(fieldUnit->fieldUnit.dataNode, &data) == ERR)
        {
            aml_node_deinit(&data);
            return ERR;
        }
        aml_node_deinit(&data);
        return 0;
    }
    default:
        LOG_ERR("invalid field node type %d\n", fieldUnit->fieldUnit.type);
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_field_unit_access(aml_node_t* fieldUnit, aml_node_t* data, aml_access_direction_t direction)
{
    if (fieldUnit->fieldUnit.type == AML_FIELD_UNIT_BANK_FIELD)
    {
        aml_node_t bankValue = AML_NODE_CREATE(AML_NODE_NONE);
        if (aml_node_init_integer(&bankValue, fieldUnit->fieldUnit.bankValue) == ERR)
        {
            return ERR;
        }

        if (aml_field_unit_store(fieldUnit->fieldUnit.bank, &bankValue) == ERR)
        {
            aml_node_deinit(&bankValue);
            return ERR;
        }
        aml_node_deinit(&bankValue);
    }

    aml_bit_size_t accessSize;
    if (aml_get_access_size(fieldUnit->fieldUnit.bitSize, fieldUnit->fieldUnit.flags.accessType,
            fieldUnit->fieldUnit.regionSpace, &accessSize) == ERR)
    {
        return ERR;
    }

    LOG_DEBUG("%s field '%.*s' of size %u bits with access size %u bits from opregion space '%s'\n",
        direction == AML_ACCESS_READ ? "reading" : "writing to", AML_NAME_LENGTH, fieldUnit->segment,
        fieldUnit->fieldUnit.bitSize, accessSize, aml_region_space_to_string(fieldUnit->fieldUnit.regionSpace));

    uint64_t byteOffset = aml_get_aligned_byte_offset(fieldUnit->fieldUnit.bitOffset, accessSize);

    uint64_t currentPos = 0;
    while (currentPos < fieldUnit->fieldUnit.bitSize)
    {
        aml_bit_size_t inAccessOffset = (fieldUnit->fieldUnit.bitOffset + currentPos) & (accessSize - 1);
        aml_bit_size_t bitsToAccess = MIN(fieldUnit->fieldUnit.bitSize - currentPos, accessSize - inAccessOffset);
        uint64_t mask = (bitsToAccess >= 64) ? UINT64_MAX : ((UINT64_C(1) << bitsToAccess) - 1);

        switch (direction)
        {
        case AML_ACCESS_READ:
        {
            uint64_t value;
            if (aml_generic_field_read_at(fieldUnit, accessSize, byteOffset, &value) == ERR)
            {
                return ERR;
            }

            value = (value >> inAccessOffset) & mask;

            if (aml_node_put_bits_at(data, value, currentPos, bitsToAccess) == ERR)
            {
                return ERR;
            }
        }
        break;
        case AML_ACCESS_WRITE:
        {
            uint64_t value;
            switch (fieldUnit->fieldUnit.flags.updateRule)
            {
            case AML_UPDATE_RULE_PRESERVE:
            {
                if (aml_generic_field_read_at(fieldUnit, accessSize, byteOffset, &value) == ERR)
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
                LOG_ERR("invalid field update rule %d\n", fieldUnit->fieldUnit.flags.updateRule);
                errno = EINVAL;
                return ERR;
            }

            value &= ~(mask << inAccessOffset);

            uint64_t newValue;
            if (aml_node_get_bits_at(data, currentPos, bitsToAccess, &newValue) == ERR)
            {
                return ERR;
            }
            value |= (newValue & mask) << inAccessOffset;

            if (aml_generic_field_write_at(fieldUnit, accessSize, byteOffset, value) == ERR)
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

uint64_t aml_field_unit_load(aml_node_t* fieldUnit, aml_node_t* out)
{
    if (fieldUnit == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t byteSize = (fieldUnit->fieldUnit.bitSize + 7) / 8;
    if (byteSize > sizeof(uint64_t))
    {
        if (aml_node_init_buffer_empty(out, byteSize) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_node_init_integer(out, 0) == ERR)
        {
            return ERR;
        }
    }

    mutex_t* globalMutex = NULL;
    if (fieldUnit->fieldUnit.flags.lockRule == AML_LOCK_RULE_LOCK)
    {
        globalMutex = aml_global_mutex_get();
        mutex_acquire_recursive(globalMutex);
    }

    uint64_t result = aml_field_unit_access(fieldUnit, out, AML_ACCESS_READ);

    if (result == ERR)
    {
        aml_node_deinit(out);
    }

    if (globalMutex != NULL)
    {
        mutex_release(globalMutex);
    }

    return result;
}

uint64_t aml_field_unit_store(aml_node_t* fieldUnit, aml_node_t* in)
{
    if (fieldUnit == NULL || in == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (in->type != AML_DATA_INTEGER && in->type != AML_DATA_BUFFER)
    {
        LOG_ERR("cannot write field '%.*s' with data of type '%s'\n", AML_NAME_LENGTH, fieldUnit->segment,
            aml_data_type_to_string(in->type));
        errno = EINVAL;
        return ERR;
    }

    mutex_t* globalMutex = NULL;
    if (fieldUnit->fieldUnit.flags.lockRule == AML_LOCK_RULE_LOCK)
    {
        globalMutex = aml_global_mutex_get();
        mutex_acquire_recursive(globalMutex);
    }

    uint64_t result = aml_field_unit_access(fieldUnit, in, AML_ACCESS_WRITE);

    if (globalMutex != NULL)
    {
        mutex_release(globalMutex);
    }

    return result;
}
