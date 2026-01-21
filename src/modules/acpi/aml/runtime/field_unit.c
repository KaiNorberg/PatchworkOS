#include <kernel/cpu/port.h>
#include <kernel/log/log.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/vmm.h>
#include <modules/acpi/aml/predefined.h>
#include <modules/acpi/aml/runtime/access_type.h>
#include <modules/acpi/aml/runtime/evaluate.h>
#include <modules/acpi/aml/runtime/field_unit.h>
#include <modules/acpi/aml/runtime/method.h>
#include <modules/acpi/aml/state.h>
#include <modules/acpi/aml/to_string.h>
#include <modules/drivers/pci/config.h>

#include <errno.h>
#include <stdint.h>

typedef struct aml_region_handler
{
    uint64_t (*read)(aml_state_t* state, aml_opregion_t* opregion, uint64_t address, aml_bit_size_t accessSize,
        uint64_t* out);
    uint64_t (*write)(aml_state_t* state, aml_opregion_t* opregion, uint64_t address, aml_bit_size_t accessSize,
        uint64_t value);
} aml_region_handler_t;

static void* aml_ensure_mem_is_mapped(uint64_t address, aml_bit_size_t accessSize)
{
    size_t accessBytes = (accessSize + 7) / 8;

    bool crossesBoundary = ((uintptr_t)address & (PAGE_SIZE - 1)) + accessBytes > PAGE_SIZE;

    for (uint64_t page = 0; page < (crossesBoundary ? 2 : 1); page++)
    {
        phys_addr_t physAddr = (phys_addr_t)address + page * PAGE_SIZE;
        void* virtAddt = (void*)PML_LOWER_TO_HIGHER(physAddr);
        if (vmm_map(NULL, virtAddt, physAddr, PAGE_SIZE, PML_GLOBAL | PML_WRITE | PML_PRESENT, NULL, NULL) == NULL)
        {
            LOG_ERR("failed to map physical address %p for opregion access\n", physAddr);
            errno = EIO;
            return NULL;
        }
    }

    return (void*)PML_LOWER_TO_HIGHER(address);
}

static uint64_t aml_system_mem_read(aml_state_t* state, aml_opregion_t* opregion, uintptr_t address,
    aml_bit_size_t accessSize, uint64_t* out)
{
    UNUSED(state);
    UNUSED(opregion);

    void* virtAddr = NULL;
    if (address >= VMM_IDENTITY_MAPPED_MIN)
    {
        virtAddr = (void*)address;
    }
    else
    {
        virtAddr = aml_ensure_mem_is_mapped(address, accessSize);
        if (virtAddr == NULL)
        {
            return ERR;
        }
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

static uint64_t aml_system_mem_write(aml_state_t* state, aml_opregion_t* opregion, uint64_t address,
    aml_bit_size_t accessSize, uint64_t value)
{
    UNUSED(state);
    UNUSED(opregion);

    void* virtAddr = NULL;
    if (address >= VMM_IDENTITY_MAPPED_MIN)
    {
        virtAddr = (void*)address;
    }
    else
    {
        virtAddr = aml_ensure_mem_is_mapped(address, accessSize);
        if (virtAddr == NULL)
        {
            return ERR;
        }
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

static uint64_t aml_system_io_read(aml_state_t* state, aml_opregion_t* opregion, uint64_t address,
    aml_bit_size_t accessSize, uint64_t* out)
{
    UNUSED(state);
    UNUSED(opregion);

    switch (accessSize)
    {
    case 8:
        *out = in8(address);
        break;
    case 16:
        *out = in16(address);
        break;
    case 32:
        *out = in32(address);
        break;
    default:
        LOG_ERR("unable to read opregion with access size %u\n", accessSize);
        errno = ENOSYS;
        return ERR;
    }
    return 0;
}

static uint64_t aml_system_io_write(aml_state_t* state, aml_opregion_t* opregion, uint64_t address,
    aml_bit_size_t accessSize, uint64_t value)
{
    UNUSED(state);
    UNUSED(opregion);

    switch (accessSize)
    {
    case 8:
        out8(address, (uint8_t)value);
        break;
    case 16:
        out16(address, (uint16_t)value);
        break;
    case 32:
        out32(address, (uint32_t)value);
        break;
    default:
        LOG_ERR("unable to write opregion with access size %u\n", accessSize);
        errno = ENOSYS;
        return ERR;
    }
    return 0;
}

static uint64_t aml_pci_get_params(aml_state_t* state, aml_opregion_t* opregion, pci_segment_group_t* segmentGroup,
    pci_bus_t* bus, pci_slot_t* slot, pci_function_t* function)
{
    UNUSED(state);
    // Note that the aml_object_find function will recursively search parent scopes.

    // We assume zero for all parameters if the corresponding object is not found.
    *segmentGroup = 0;
    *bus = 0;
    *slot = 0;
    *function = 0;

    aml_object_t* location = CONTAINER_OF(opregion, aml_object_t, opregion);

    // See section 6.1.1 of the ACPI specification.
    aml_object_t* adrObject = aml_namespace_find(&state->overlay, location, AML_NAME('_', 'A', 'D', 'R'));
    if (adrObject != NULL)
    {
        UNREF_DEFER(adrObject);

        aml_object_t* adrResult = aml_evaluate(state, adrObject, AML_INTEGER);
        if (adrResult == NULL)
        {
            LOG_ERR("failed to evaluate _ADR for opregion '%s'\n", AML_NAME_TO_STRING(location->name));
            return ERR;
        }
        aml_uint_t adrValue = adrResult->integer.value;
        UNREF(adrResult);

        *slot = adrValue & 0x0000FFFF;             // Low word is slot
        *function = (adrValue >> 16) & 0x0000FFFF; // High word is function
    }

    // Section 6.5.5 of the ACPI specification.
    aml_object_t* bbnObject = aml_namespace_find(&state->overlay, location, AML_NAME('_', 'B', 'B', 'N'));
    if (bbnObject != NULL)
    {
        UNREF_DEFER(bbnObject);

        aml_object_t* bbnResult = aml_evaluate(state, bbnObject, AML_INTEGER);
        if (bbnResult == NULL)
        {
            LOG_ERR("failed to evaluate _BBN for opregion '%s'\n", AML_NAME_TO_STRING(location->name));
            return ERR;
        }
        aml_uint_t bbnValue = bbnResult->integer.value;
        UNREF(bbnResult);

        // Lower 8 bits is the bus number.
        *bus = bbnValue & 0xFF;
    }

    // Section 6.5.6 of the ACPI specification.
    aml_object_t* segObject = aml_namespace_find(&state->overlay, location, AML_NAME('_', 'S', 'E', 'G'));
    if (segObject != NULL)
    {
        UNREF_DEFER(segObject);

        aml_object_t* segResult = aml_evaluate(state, segObject, AML_INTEGER);
        if (segResult == NULL)
        {
            LOG_ERR("failed to evaluate _SEG for opregion '%s'\n", AML_NAME_TO_STRING(location->name));
            return ERR;
        }
        aml_uint_t segValue = segResult->integer.value;
        UNREF(segResult);

        // Lower 16 bits is the segment group number.
        *segmentGroup = segValue & 0xFFFF;
    }

    return 0;
}

static uint64_t aml_pci_config_read(aml_state_t* state, aml_opregion_t* opregion, uint64_t address,
    aml_bit_size_t accessSize, uint64_t* out)
{
    UNUSED(state);

    pci_segment_group_t segmentGroup;
    pci_bus_t bus;
    pci_slot_t slot;
    pci_function_t function;
    if (aml_pci_get_params(state, opregion, &segmentGroup, &bus, &slot, &function) == ERR)
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

static uint64_t aml_pci_config_write(aml_state_t* state, aml_opregion_t* opregion, uint64_t address,
    aml_bit_size_t accessSize, uint64_t value)
{
    pci_segment_group_t segmentGroup;
    pci_bus_t bus;
    pci_slot_t slot;
    pci_function_t function;
    if (aml_pci_get_params(state, opregion, &segmentGroup, &bus, &slot, &function) == ERR)
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

static inline uint64_t aml_opregion_read(aml_state_t* state, aml_opregion_t* opregion, uint64_t address,
    aml_bit_size_t accessSize, uint64_t* out)
{
    if (out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (opregion->space >= ARRAY_SIZE(regionHandlers) || regionHandlers[opregion->space].read == NULL)
    {
        LOG_ERR("unimplemented opregion read with opregion space '%s'\n", aml_region_space_to_string(opregion->space));
        errno = ENOSYS;
        return ERR;
    }
    return regionHandlers[opregion->space].read(state, opregion, address, accessSize, out);
}

static inline uint64_t aml_opregion_write(aml_state_t* state, aml_opregion_t* opregion, uint64_t address,
    aml_bit_size_t accessSize, uint64_t value)
{
    if (opregion->space >= ARRAY_SIZE(regionHandlers) || regionHandlers[opregion->space].write == NULL)
    {
        LOG_ERR("unimplemented opregion write with opregion space '%s'\n", aml_region_space_to_string(opregion->space));
        errno = ENOSYS;
        return ERR;
    }
    return regionHandlers[opregion->space].write(state, opregion, address, accessSize, value);
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

static uint64_t aml_generic_field_read_at(aml_state_t* state, aml_field_unit_t* fieldUnit, aml_bit_size_t accessSize,
    uint64_t byteOffset, uint64_t* out)
{
    switch (fieldUnit->fieldType)
    {
    case AML_FIELD_UNIT_FIELD:
    case AML_FIELD_UNIT_BANK_FIELD:
    {
        aml_opregion_t* opregion = fieldUnit->opregion;
        uintptr_t address = opregion->offset + byteOffset;
        return aml_opregion_read(state, opregion, address, accessSize, out);
    }
    case AML_FIELD_UNIT_INDEX_FIELD:
    {
        aml_object_t* temp = aml_object_new();
        if (temp == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(temp);

        if (aml_integer_set(temp, byteOffset) == ERR)
        {
            return ERR;
        }

        if (aml_field_unit_store(state, fieldUnit->index, temp) == ERR)
        {
            return ERR;
        }

        aml_object_clear(temp);

        if (aml_field_unit_load(state, fieldUnit->data, temp) == ERR)
        {
            return ERR;
        }

        assert(temp->type == AML_INTEGER);

        *out = temp->integer.value;
        return 0;
    }
    default:
        LOG_ERR("invalid field object type %d\n", fieldUnit->type);
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_generic_field_write_at(aml_state_t* state, aml_field_unit_t* fieldUnit, aml_bit_size_t accessSize,
    uint64_t byteOffset, uint64_t value)
{
    switch (fieldUnit->fieldType)
    {
    case AML_FIELD_UNIT_FIELD:
    case AML_FIELD_UNIT_BANK_FIELD:
    {
        aml_opregion_t* opregion = fieldUnit->opregion;
        uintptr_t address = opregion->offset + byteOffset;
        return aml_opregion_write(state, opregion, address, accessSize, value);
    }
    case AML_FIELD_UNIT_INDEX_FIELD:
    {
        aml_object_t* temp = aml_object_new();
        if (temp == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(temp);

        if (aml_integer_set(temp, byteOffset) == ERR)
        {
            return ERR;
        }

        if (aml_field_unit_store(state, fieldUnit->index, temp) == ERR)
        {
            return ERR;
        }

        aml_object_clear(temp);

        if (aml_integer_set(temp, value) == ERR)
        {
            return ERR;
        }

        if (aml_field_unit_store(state, fieldUnit->data, temp) == ERR)
        {
            return ERR;
        }
        return 0;
    }
    default:
        LOG_ERR("invalid field object type %d\n", fieldUnit->type);
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_field_unit_access(aml_state_t* state, aml_field_unit_t* fieldUnit, aml_object_t* data,
    aml_access_direction_t direction)
{
    // The integer revision handling is enterily done by the aml_get_access_size function, so we dont need to
    // do anything special here.

    uint64_t result = 0;
    aml_mutex_t* globalMutex = NULL;
    if (fieldUnit->fieldFlags.lockRule == AML_LOCK_RULE_LOCK)
    {
        globalMutex = aml_gl_get();
        if (aml_mutex_acquire(&globalMutex->mutex, globalMutex->syncLevel, CLOCKS_NEVER) == ERR)
        {
            return ERR;
        }
    }

    /// @todo "An Operation Region object implicitly supports Mutex synchronization. Updates to the object, or a Field
    /// data object for the region, will automatically synchronize on the Operation Region object;" - Section 19.6.100

    if (fieldUnit->fieldType == AML_FIELD_UNIT_BANK_FIELD)
    {
        if (aml_field_unit_store(state, fieldUnit->bank, fieldUnit->bankValue) == ERR)
        {
            result = ERR;
            goto cleanup;
        }
    }

    aml_region_space_t regionSpace;
    if (fieldUnit->fieldType == AML_FIELD_UNIT_INDEX_FIELD)
    {
        regionSpace = fieldUnit->data->opregion->space;
    }
    else
    {
        regionSpace = fieldUnit->opregion->space;
    }

    aml_bit_size_t accessSize;
    if (aml_get_access_size(fieldUnit->bitSize, fieldUnit->fieldFlags.accessType, regionSpace, &accessSize) == ERR)
    {
        result = ERR;
        goto cleanup;
    }

    uint64_t byteOffset = aml_get_aligned_byte_offset(fieldUnit->bitOffset, accessSize);

    uint64_t currentPos = 0;
    while (currentPos < fieldUnit->bitSize)
    {
        aml_bit_size_t inAccessOffset = (fieldUnit->bitOffset + currentPos) & (accessSize - 1);
        aml_bit_size_t bitsToAccess = MIN(fieldUnit->bitSize - currentPos, accessSize - inAccessOffset);
        uint64_t mask = (bitsToAccess >= 64) ? UINT64_MAX : ((1ULL << bitsToAccess) - 1);

        switch (direction)
        {
        case AML_ACCESS_READ:
        {
            uint64_t value = 0;
            if (aml_generic_field_read_at(state, fieldUnit, accessSize, byteOffset, &value) == ERR)
            {
                result = ERR;
                goto cleanup;
            }

            value = (value >> inAccessOffset) & mask;

            // We treat value as a buffer of 8 bytes.
            if (aml_object_set_bits_at(data, currentPos, bitsToAccess, (uint8_t*)&value) == ERR)
            {
                result = ERR;
                goto cleanup;
            }
        }
        break;
        case AML_ACCESS_WRITE:
        {
            uint64_t value;
            switch (fieldUnit->fieldFlags.updateRule)
            {
            case AML_UPDATE_RULE_PRESERVE:
            {
                if (aml_generic_field_read_at(state, fieldUnit, accessSize, byteOffset, &value) == ERR)
                {
                    result = ERR;
                    goto cleanup;
                }
            }
            break;
            case AML_UPDATE_RULE_WRITE_AS_ONES:
                value = UINT64_MAX;
                break;
            case AML_UPDATE_RULE_WRITE_AS_ZEROS:
                value = 0;
                break;
            default:
                LOG_ERR("invalid field update rule %d\n", fieldUnit->fieldFlags.updateRule);
                errno = EINVAL;
                result = ERR;
                goto cleanup;
            }

            value &= ~(mask << inAccessOffset);

            uint64_t newValue;
            // We treat newValue as a buffer of 8 bytes.
            if (aml_object_get_bits_at(data, currentPos, bitsToAccess, (uint8_t*)&newValue) == ERR)
            {
                result = ERR;
                goto cleanup;
            }
            value |= (newValue & mask) << inAccessOffset;

            if (aml_generic_field_write_at(state, fieldUnit, accessSize, byteOffset, value) == ERR)
            {
                result = ERR;
                goto cleanup;
            }
        }
        break;
        default:
            LOG_ERR("invalid field access direction %d\n", direction);
            errno = EINVAL;
            result = ERR;
            goto cleanup;
        }

        currentPos += bitsToAccess;
        byteOffset += accessSize / 8;
    }

cleanup:
    if (globalMutex != NULL)
    {
        if (aml_mutex_release(&globalMutex->mutex) == ERR)
        {
            result = ERR;
        }
    }
    return result;
}

uint64_t aml_field_unit_load(aml_state_t* state, aml_field_unit_t* fieldUnit, aml_object_t* out)
{
    if (fieldUnit == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t byteSize = (fieldUnit->bitSize + 7) / 8;
    if (byteSize > aml_integer_byte_size())
    {
        if (aml_buffer_set_empty(out, byteSize) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_integer_set(out, 0) == ERR)
        {
            return ERR;
        }
    }

    return aml_field_unit_access(state, fieldUnit, out, AML_ACCESS_READ);
}

uint64_t aml_field_unit_store(aml_state_t* state, aml_field_unit_t* fieldUnit, aml_object_t* in)
{
    if (fieldUnit == NULL || in == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_type_t type = in->type;
    if (type != AML_INTEGER && type != AML_BUFFER)
    {
        LOG_ERR("cannot write to field unit with data of type '%s'\n", aml_type_to_string(type));
        errno = EINVAL;
        return ERR;
    }

    return aml_field_unit_access(state, fieldUnit, in, AML_ACCESS_WRITE);
}
