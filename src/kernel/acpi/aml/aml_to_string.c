#include "aml_to_string.h"

#include <stdio.h>

const char* aml_node_type_to_string(aml_node_type_t type)
{
    switch (type)
    {
    case AML_NODE_NONE:
        return "None";
    case AML_NODE_PREDEFINED:
        return "Predefined";
    case AML_NODE_DEVICE:
        return "Device";
    case AML_NODE_PROCESSOR:
        return "Processor";
    case AML_NODE_THERMAL_ZONE:
        return "ThermalZone";
    case AML_NODE_POWER_RESOURCE:
        return "PowerResource";
    case AML_NODE_OPREGION:
        return "OpRegion";
    case AML_NODE_FIELD:
        return "Field";
    case AML_NODE_METHOD:
        return "Method";
    case AML_NODE_NAME:
        return "Name";
    case AML_NODE_MUTEX:
        return "Mutex";
    default:
        return "Unknown";
    }
}

const char* aml_region_space_to_string(aml_region_space_t space)
{
    switch (space)
    {
    case AML_REGION_SYSTEM_MEMORY:
        return "SystemMemory";
    case AML_REGION_SYSTEM_IO:
        return "SystemIO";
    case AML_REGION_PCI_CONFIG:
        return "PCIConfig";
    case AML_REGION_EMBEDDED_CONTROL:
        return "EmbeddedControl";
    case AML_REGION_SM_BUS:
        return "SMBus";
    case AML_REGION_SYSTEM_CMOS:
        return "SystemCmos";
    case AML_REGION_PCI_BAR_TARGET:
        return "PCIBarTarget";
    case AML_REGION_IPMI:
        return "IPMI";
    case AML_REGION_GENERAL_PURPOSE_IO:
        return "GeneralPurposeIO";
    case AML_REGION_GENERIC_SERIAL_BUS:
        return "GenericSerialBus";
    case AML_REGION_PCC:
        return "PCC";
    default:
        if (space >= AML_REGION_OEM_MIN && space <= AML_REGION_OEM_MAX)
        {
            return "OEM";
        }
        return "Unknown";
    }
}

const char* aml_access_type_to_string(aml_access_type_t accessType)
{
    switch (accessType)
    {
    case AML_ACCESS_TYPE_ANY:
        return "AnyAcc";
    case AML_ACCESS_TYPE_BYTE:
        return "ByteAcc";
    case AML_ACCESS_TYPE_WORD:
        return "WordAcc";
    case AML_ACCESS_TYPE_DWORD:
        return "DWordAcc";
    case AML_ACCESS_TYPE_QWORD:
        return "QWordAcc";
    case AML_ACCESS_TYPE_BUFFER:
        return "BufferAcc";
    default:
        return "Unknown";
    }
}

const char* aml_lock_rule_to_string(aml_lock_rule_t lockRule)
{
    switch (lockRule)
    {
    case AML_LOCK_RULE_NO_LOCK:
        return "NoLock";
    case AML_LOCK_RULE_LOCK:
        return "Lock";
    default:
        return "Unknown";
    }
}

const char* aml_update_rule_to_string(aml_update_rule_t updateRule)
{
    switch (updateRule)
    {
    case AML_UPDATE_RULE_PRESERVE:
        return "Preserve";
    case AML_UPDATE_RULE_WRITE_AS_ONES:
        return "WriteAsOnes";
    case AML_UPDATE_RULE_WRITE_AS_ZEROS:
        return "WriteAsZeros";
    default:
        return "Unknown";
    }
}

const char* aml_data_object_to_string(aml_data_object_t* dataObject)
{
    static char buffer[256];
    switch (dataObject->type)
    {
    case AML_DATA_INTEGER:
        snprintf(buffer, sizeof(buffer), "0x%llx", dataObject->integer);
        return buffer;
    case AML_DATA_STRING:
        snprintf(buffer, sizeof(buffer), "\"%s\"", dataObject->string.content);
        return buffer;
    case AML_DATA_BUFFER:
        if (dataObject->buffer.length <= 16)
        {
            int offset = 0;
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[");
            for (uint64_t i = 0; i < dataObject->buffer.length; i++)
            {
                if (i > 0)
                {
                    offset += snprintf(buffer + offset, sizeof(buffer) - offset, " ");
                }
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "0x%02x", dataObject->buffer.content[i]);
            }
            snprintf(buffer + offset, sizeof(buffer) - offset, "]");
        }
        else
        {
            int offset = 0;
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[");
            for (uint64_t i = 0; i < 8; i++)
            {
                if (i > 0)
                {
                    offset += snprintf(buffer + offset, sizeof(buffer) - offset, " ");
                }
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "0x%02x", dataObject->buffer.content[i]);
            }
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, " ... ");
            for (uint64_t i = dataObject->buffer.length - 8; i < dataObject->buffer.length; i++)
            {
                if (i > dataObject->buffer.length - 8)
                {
                    offset += snprintf(buffer + offset, sizeof(buffer) - offset, " ");
                }
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "0x%02x", dataObject->buffer.content[i]);
            }
            snprintf(buffer + offset, sizeof(buffer) - offset, "] (Length=%llu)", dataObject->buffer.length);
        }
        return buffer;
    case AML_DATA_PACKAGE:
        snprintf(buffer, sizeof(buffer), "Package(NumElements=%llu)", dataObject->package.numElements);
        return buffer;
    default:
        return "Unknown";
    }
}

const char* aml_name_string_to_string(aml_name_string_t* nameString)
{
    static char buffer[256];
    int offset = 0;
    if (nameString->rootChar.present)
    {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\\");
    }
    for (uint64_t i = 0; i < nameString->prefixPath.depth; i++)
    {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "^");
    }
    for (uint64_t i = 0; i < nameString->namePath.segmentCount; i++)
    {
        if (i > 0)
        {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, ".");
        }
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%.*s", AML_NAME_LENGTH,
            nameString->namePath.segments[i].name);
    }
    return buffer;
}
