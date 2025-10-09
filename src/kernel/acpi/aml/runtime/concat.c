#include "concat.h"

#include "convert.h"
#include "log/log.h"

#include <errno.h>

static uint64_t aml_concat_resolve_to_integer(aml_object_t* source, uint64_t* out)
{
    if (source->type == AML_INTEGER)
    {
        *out = source->integer.value;
        return 0;
    }
    else if (source->type == AML_STRING || source->type == AML_BUFFER)
    {
        aml_object_t* temp = aml_object_new(NULL, AML_OBJECT_NONE);
        if (temp == NULL)
        {
            return ERR;
        }
        DEREF_DEFER(temp);

        if (aml_convert_source(source, temp, AML_INTEGER) == ERR)
        {
            return ERR;
        }

        *out = temp->integer.value;
        return 0;
    }
    else
    {
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_concat_resolve_to_string(aml_object_t* source, const char** outStr, aml_object_t** outTemp)
{
    if (source->type == AML_STRING)
    {
        *outStr = source->string.content;
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_INTEGER || source->type == AML_BUFFER)
    {
        aml_object_t* temp = aml_object_new(NULL, AML_OBJECT_NONE);
        if (temp == NULL)
        {
            return ERR;
        }
        DEREF_DEFER(temp);

        if (aml_convert_source(source, temp, AML_STRING) == ERR)
        {
            return ERR;
        }

        *outStr = temp->string.content;
        *outTemp = REF(temp); // Caller must deref
        return 0;
    }
    else if (source->type == AML_UNINITIALIZED)
    {
        *outStr = "Uninitialized Object";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_PACKAGE)
    {
        *outStr = "Package";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_FIELD_UNIT)
    {
        *outStr = "Field Unit";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_DEVICE)
    {
        *outStr = "Device";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_EVENT)
    {
        *outStr = "Event";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_METHOD)
    {
        *outStr = "Control Method";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_MUTEX)
    {
        *outStr = "Mutex";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_OPERATION_REGION)
    {
        *outStr = "Operation Region";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_POWER_RESOURCE)
    {
        *outStr = "Power Resource";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_PROCESSOR)
    {
        *outStr = "Processor";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_THERMAL_ZONE)
    {
        *outStr = "Thermal Zone";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_BUFFER_FIELD)
    {
        *outStr = "Buffer Field";
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_DEBUG_OBJECT)
    {
        *outStr = "Debug Object";
        *outTemp = NULL;
        return 0;
    }
    else
    {
        errno = EINVAL;
        return ERR;
    }
}

static uint64_t aml_concat_resolve_to_buffer(aml_object_t* source, uint8_t** outBuf, uint64_t* outLen, aml_object_t** outTemp)
{
    if (source->type == AML_BUFFER)
    {
        *outBuf = source->buffer.content;
        *outLen = source->buffer.length;
        *outTemp = NULL;
        return 0;
    }
    else if (source->type == AML_INTEGER || source->type == AML_STRING)
    {
        aml_object_t* temp = aml_object_new(NULL, AML_OBJECT_NONE);
        if (temp == NULL)
        {
            return ERR;
        }
        DEREF_DEFER(temp);

        if (aml_convert_source(source, temp, AML_BUFFER) == ERR)
        {
            return ERR;
        }

        *outBuf = temp->buffer.content;
        *outLen = temp->buffer.length;
        *outTemp = REF(temp); // Caller must deref
        return 0;
    }
    else
    {
        // The spec seems a bit vague on this. But my assumption is that when resolving other types to a buffer,
        // we first convert them to a string, then take the string's bytes as the buffer content.
        const char* str;
        if (aml_concat_resolve_to_string(source, &str, outTemp) == ERR)
        {
            return ERR;
        }

        *outBuf = (uint8_t*)str;
        *outLen = strlen(str);
        return 0;
    }
}

static uint64_t aml_concat_integer(aml_object_t* source1, aml_object_t* source2, aml_object_t* result)
{
    assert(source1->type == AML_INTEGER);
    uint64_t value1 = source1->integer.value;
    uint64_t value2;
    if (aml_concat_resolve_to_integer(source2, &value2) == ERR)
    {
        return ERR;
    }

    if (aml_buffer_init_empty(result, sizeof(uint64_t) * 2) == ERR)
    {
        return ERR;
    }
    memcpy(result->buffer.content, &value1, sizeof(uint64_t));
    memcpy(result->buffer.content + sizeof(uint64_t), &value2, sizeof(uint64_t));

    return 0;
}

static uint64_t aml_concat_string(aml_object_t* source1, aml_object_t* source2, aml_object_t* result)
{
    assert(source1->type == AML_STRING);
    const char* str1 = source1->string.content;
    const char* str2;
    aml_object_t* temp2 = NULL;
    if (aml_concat_resolve_to_string(source2, &str2, &temp2) == ERR)
    {
        return ERR;
    }
    DEREF_DEFER(temp2);

    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    if (aml_string_init_empty(result, len1 + len2) == ERR)
    {
        return ERR;
    }
    memcpy(result->string.content, str1, len1);
    memcpy(result->string.content + len1, str2, len2);

    return 0;
}

static uint64_t aml_concat_buffer(aml_object_t* source1, aml_object_t* source2, aml_object_t* result)
{
    assert(source1->type == AML_BUFFER);
    uint8_t* buf1 = source1->buffer.content;
    uint64_t len1 = source1->buffer.length;
    uint8_t* buf2;
    uint64_t len2;
    aml_object_t* temp2 = NULL;
    if (aml_concat_resolve_to_buffer(source2, &buf2, &len2, &temp2) == ERR)
    {
        return ERR;
    }
    DEREF_DEFER(temp2);

    if (aml_buffer_init_empty(result, len1 + len2) == ERR)
    {
        return ERR;
    }
    memcpy(result->buffer.content, buf1, len1);
    memcpy(result->buffer.content + len1, buf2, len2);

    return 0;
}

static uint64_t aml_concat_other_types(aml_object_t* source1, aml_object_t* source2, aml_object_t* result)
{
    const char* str1;
    aml_object_t* temp1 = NULL;
    if (aml_concat_resolve_to_string(source1, &str1, &temp1) == ERR)
    {
        return ERR;
    }
    DEREF_DEFER(temp1);

    const char* str2;
    aml_object_t* temp2 = NULL;
    if (aml_concat_resolve_to_string(source2, &str2, &temp2) == ERR)
    {
        return ERR;
    }
    DEREF_DEFER(temp2);

    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    if (aml_string_init_empty(result, len1 + len2) == ERR)
    {
        return ERR;
    }
    memcpy(result->string.content, str1, len1);
    memcpy(result->string.content + len1, str2, len2);

    return 0;
}

uint64_t aml_concat(aml_object_t* source1, aml_object_t* source2, aml_object_t* result)
{
    if (source1 == NULL || source2 == NULL || result == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (source1->type == AML_UNINITIALIZED || source2->type == AML_UNINITIALIZED)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (source1->type)
    {
    case AML_INTEGER:
        return aml_concat_integer(source1, source2, result);
    case AML_STRING:
        return aml_concat_string(source1, source2, result);
    case AML_BUFFER:
        return aml_concat_buffer(source1, source2, result);
    default:
        return aml_concat_other_types(source1, source2, result);
    }
}
