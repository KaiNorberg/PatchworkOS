#include "aml_to_string.h"

#include <stdio.h>

const char* aml_data_type_to_string(aml_data_type_t type)
{
    switch (type)
    {
    case AML_DATA_UNINITALIZED:
        return "Uninitialized";
    case AML_DATA_BUFFER:
        return "Buffer";
    case AML_DATA_BUFFER_FIELD:
        return "BufferField";
    case AML_DATA_DEBUG_OBJECT:
        return "DebugObject";
    case AML_DATA_DEVICE:
        return "Device";
    case AML_DATA_EVENT:
        return "Event";
    case AML_DATA_FIELD_UNIT:
        return "FieldUnit";
    case AML_DATA_INTEGER:
        return "Integer";
    case AML_DATA_INTEGER_CONSTANT:
        return "IntegerConstant";
    case AML_DATA_METHOD:
        return "Method";
    case AML_DATA_MUTEX:
        return "Mutex";
    case AML_DATA_OBJECT_REFERENCE:
        return "ObjectReference";
    case AML_DATA_OPERATION_REGION:
        return "OperationRegion";
    case AML_DATA_PACKAGE:
        return "Package";
    case AML_DATA_POWER_RESOURCE:
        return "PowerResource";
    case AML_DATA_PROCESSOR:
        return "Processor";
    case AML_DATA_RAW_DATA_BUFFER:
        return "RawDataBuffer";
    case AML_DATA_STRING:
        return "String";
    case AML_DATA_THERMAL_ZONE:
        return "ThermalZone";
    case AML_DATA_UNRESOLVED:
        return "Unresolved";
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

const char* aml_object_to_string(aml_object_t* object)
{
    static char buffer[256];
    if (object == NULL)
    {
        return "Unknown";
    }

    memset(buffer, 0, sizeof(buffer));
    switch (object->type)
    {
    case AML_DATA_UNINITALIZED:
        snprintf(buffer, sizeof(buffer), "Uninitialized");
        return buffer;
    case AML_DATA_BUFFER:
        snprintf(buffer, sizeof(buffer), "Buffer(Length=%llu, Content=0x", object->buffer.length);
        for (uint64_t i = 0; i < object->buffer.length && i < 8; i++)
        {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "%02x", object->buffer.content[i]);
        }
        if (object->buffer.length > 8)
        {
            snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), "...");
        }
        snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer), ")");
        return buffer;
    case AML_DATA_BUFFER_FIELD:
        snprintf(buffer, sizeof(buffer), "BufferField(BitOffset=%llu, BitSize=%llu)", object->bufferField.bitOffset,
            object->bufferField.bitSize);
        return buffer;
    case AML_DATA_DEVICE:
        snprintf(buffer, sizeof(buffer), "Device");
        return buffer;
    case AML_DATA_FIELD_UNIT:
        snprintf(buffer, sizeof(buffer), "FieldUnit(Type=%d, BitOffset=%llu, BitSize=%llu)", object->fieldUnit.type,
            object->fieldUnit.bitOffset, object->fieldUnit.bitSize);
        return buffer;
    case AML_DATA_INTEGER:
        snprintf(buffer, sizeof(buffer), "Integer(0x%x)", object->integer.value);
        return buffer;
    case AML_DATA_INTEGER_CONSTANT:
        snprintf(buffer, sizeof(buffer), "IntegerConstant(0x%x)", object->integerConstant.value);
        return buffer;
    case AML_DATA_METHOD:
        snprintf(buffer, sizeof(buffer), "Method(ArgCount=0x%x, Start=0x%llx, End=0x%llx)",
            object->method.flags.argCount, object->method.start, object->method.end);
        return buffer;
    case AML_DATA_MUTEX:
        snprintf(buffer, sizeof(buffer), "Mutex(SyncLevel=%d)", object->mutex.syncLevel);
        return buffer;
    case AML_DATA_OBJECT_REFERENCE:
        if (object->objectReference.target != NULL)
        {
            snprintf(buffer, sizeof(buffer), "ObjectReference(Target='%s')", object->objectReference.target->segment);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "ObjectReference(Target=NULL)");
        }
        return buffer;
    case AML_DATA_OPERATION_REGION:
        snprintf(buffer, sizeof(buffer), "OperationRegion(Space=%s, Offset=0x%llx, Length=%u)",
            aml_region_space_to_string(object->opregion.space), object->opregion.offset, object->opregion.length);
        return buffer;
    case AML_DATA_PACKAGE:
        snprintf(buffer, sizeof(buffer), "Package(Length=%llu)", object->package.length);
        return buffer;
    case AML_DATA_STRING:
    {
        uint64_t len = strlen(object->string.content);
        if (len <= 32)
        {
            snprintf(buffer, sizeof(buffer), "String(\"%.*s\")", len, object->string.content);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "String(\"%.*s...\")", 29, len, object->string.content);
        }
        return buffer;
    }
    case AML_DATA_UNRESOLVED:
        snprintf(buffer, sizeof(buffer), "Unresolved");
        return buffer;
    default:
        snprintf(buffer, sizeof(buffer), "Unknown(Type=%d)", object->type);
        return buffer;
    }
}

const char* aml_name_string_to_string(aml_name_string_t* nameString)
{
    static char buffer[256];
    memset(buffer, 0, sizeof(buffer));

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
