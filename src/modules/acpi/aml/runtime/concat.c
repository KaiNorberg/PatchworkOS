#include <kernel/acpi/aml/runtime/concat.h>

#include <kernel/acpi/aml/runtime/convert.h>
#include <kernel/log/log.h>
#include <sys/status.h>

static status_t aml_concat_resolve_to_integer(aml_state_t* state, aml_object_t* source, aml_uint_t* out)
{
    if (source->type == AML_INTEGER)
    {
        *out = source->integer.value;
        return OK;
    }
    
    if (source->type == AML_STRING || source->type == AML_BUFFER)
    {
        aml_object_t* temp = NULL;
        status_t status = aml_convert_source(state, source, &temp, AML_INTEGER);
        if (IS_ERR(status))
        {
            return status;
        }

        *out = temp->integer.value;
        UNREF(temp);
        return OK;
    }

    return ERR(ACPI, INVAL);
}

static status_t aml_concat_resolve_to_string(aml_state_t* state, aml_object_t* source, const char** outStr,
    aml_object_t** outTemp)
{
    switch (source->type)
    {
    case AML_STRING:
        *outStr = source->string.content;
        *outTemp = NULL;
        return OK;
    case AML_INTEGER:
    case AML_BUFFER:
    {
        aml_object_t* temp = NULL;
        status_t status = aml_convert_source(state, source, &temp, AML_STRING);
        if (IS_ERR(status))
        {
            return status;
        }

        *outStr = temp->string.content;
        *outTemp = temp; // Caller must deref
        return OK;
    }
    case AML_UNINITIALIZED:
        *outStr = "Uninitialized Object";
        *outTemp = NULL;
        return OK;
    case AML_PACKAGE:
        *outStr = "Package";
        *outTemp = NULL;
        return OK;
    case AML_FIELD_UNIT:
        *outStr = "Field Unit";
        *outTemp = NULL;
        return OK;
    case AML_DEVICE:
        *outStr = "Device";
        *outTemp = NULL;
        return OK;
    case AML_EVENT:
        *outStr = "Event";
        *outTemp = NULL;
        return OK;
    case AML_METHOD:
        *outStr = "Control Method";
        *outTemp = NULL;
        return OK;
    case AML_MUTEX:
        *outStr = "Mutex";
        *outTemp = NULL;
        return OK;
    case AML_OPERATION_REGION:
        *outStr = "Operation Region";
        *outTemp = NULL;
        return OK;
    case AML_POWER_RESOURCE:
        *outStr = "Power Resource";
        *outTemp = NULL;
        return OK;
    case AML_PROCESSOR:
        *outStr = "Processor";
        *outTemp = NULL;
        return OK;
    case AML_THERMAL_ZONE:
        *outStr = "Thermal Zone";
        *outTemp = NULL;
        return OK;
    case AML_BUFFER_FIELD:
        *outStr = "Buffer Field";
        *outTemp = NULL;
        return OK;
    case AML_DEBUG_OBJECT:
        *outStr = "Debug Object";
        *outTemp = NULL;
        return OK;
    default:
        return ERR(ACPI, INVAL);
    }
}

static status_t aml_concat_resolve_to_buffer(aml_state_t* state, aml_object_t* source, uint8_t** outBuf,
    uint64_t* outLen, aml_object_t** outTemp)
{
    if (source->type == AML_BUFFER)
    {
        *outBuf = source->buffer.content;
        *outLen = source->buffer.length;
        *outTemp = NULL;
        return OK;
    }
    
    if (source->type == AML_INTEGER || source->type == AML_STRING)
    {
        aml_object_t* temp = NULL;
        status_t status = aml_convert_source(state, source, &temp, AML_BUFFER);
        if (IS_ERR(status))
        {
            return status;
        }

        *outBuf = temp->buffer.content;
        *outLen = temp->buffer.length;
        *outTemp = temp; // Caller must deref
        return OK;
    }

    // The spec seems a bit vague on this. But my assumption is that when resolving other types to a buffer,
    // we first convert them to a string, then take the string's bytes as the buffer content.
    const char* str;
    status_t status = aml_concat_resolve_to_string(state, source, &str, outTemp);
    if (IS_ERR(status))
    {
        return status;
    }

    *outBuf = (uint8_t*)str;
    *outLen = strlen(str);
    return OK;
}

static status_t aml_concat_integer(aml_state_t* state, aml_object_t* source1, aml_object_t* source2,
    aml_object_t* result)
{
    assert(source1->type == AML_INTEGER);
    aml_uint_t value1 = source1->integer.value;
    aml_uint_t value2;
    status_t status = aml_concat_resolve_to_integer(state, source2, &value2);
    if (IS_ERR(status))
    {
        return status;
    }

    status = aml_buffer_set_empty(result, aml_integer_byte_size() * 2);
    if (IS_ERR(status))
    {
        return status;
    }
    memcpy(result->buffer.content, &value1, aml_integer_byte_size());
    memcpy(result->buffer.content + aml_integer_byte_size(), &value2, aml_integer_byte_size());

    return OK;
}

static status_t aml_concat_string(aml_state_t* state, aml_object_t* source1, aml_object_t* source2,
    aml_object_t* result)
{
    assert(source1->type == AML_STRING);
    const char* str1 = source1->string.content;
    const char* str2;
    aml_object_t* temp2 = NULL;
    status_t status = aml_concat_resolve_to_string(state, source2, &str2, &temp2);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(temp2);

    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    status = aml_string_set_empty(result, len1 + len2);
    if (IS_ERR(status))
    {
        return status;
    }
    memcpy(result->string.content, str1, len1);
    memcpy(result->string.content + len1, str2, len2);

    return OK;
}

static status_t aml_concat_buffer(aml_state_t* state, aml_object_t* source1, aml_object_t* source2,
    aml_object_t* result)
{
    assert(source1->type == AML_BUFFER);
    uint8_t* buf1 = source1->buffer.content;
    uint64_t len1 = source1->buffer.length;
    uint8_t* buf2;
    uint64_t len2;
    aml_object_t* temp2 = NULL;
    status_t status = aml_concat_resolve_to_buffer(state, source2, &buf2, &len2, &temp2);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(temp2);

    status = aml_buffer_set_empty(result, len1 + len2);
    if (IS_ERR(status))
    {
        return status;
    }
    memcpy(result->buffer.content, buf1, len1);
    memcpy(result->buffer.content + len1, buf2, len2);

    return OK;
}

static status_t aml_concat_other_types(aml_state_t* state, aml_object_t* source1, aml_object_t* source2,
    aml_object_t* result)
{
    const char* str1;
    aml_object_t* temp1 = NULL;
    status_t status = aml_concat_resolve_to_string(state, source1, &str1, &temp1);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(temp1);

    const char* str2;
    aml_object_t* temp2 = NULL;
    status = aml_concat_resolve_to_string(state, source2, &str2, &temp2);
    if (IS_ERR(status))
    {
        return status;
    }
    UNREF_DEFER(temp2);

    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    status = aml_string_set_empty(result, len1 + len2);
    if (IS_ERR(status))
    {
        return status;
    }
    memcpy(result->string.content, str1, len1);
    memcpy(result->string.content + len1, str2, len2);

    return OK;
}

status_t aml_concat(aml_state_t* state, aml_object_t* source1, aml_object_t* source2, aml_object_t* result)
{
    if (source1 == NULL || source2 == NULL || result == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    if (source1->type == AML_UNINITIALIZED || source2->type == AML_UNINITIALIZED)
    {
        return ERR(ACPI, INVAL);
    }

    switch (source1->type)
    {
    case AML_INTEGER:
        return aml_concat_integer(state, source1, source2, result);
    case AML_STRING:
        return aml_concat_string(state, source1, source2, result);
    case AML_BUFFER:
        return aml_concat_buffer(state, source1, source2, result);
    default:
        return aml_concat_other_types(state, source1, source2, result);
    }
}
