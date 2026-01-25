#include <kernel/acpi/aml/runtime/mid.h>

aml_object_t* aml_mid(aml_state_t* state, aml_object_t* bufferString, aml_uint_t index, aml_uint_t length)
{
    if (state == NULL || bufferString == NULL || (bufferString->type != AML_BUFFER && bufferString->type != AML_STRING))
    {
        errno = EINVAL;
        return NULL;
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return NULL;
    }
    UNREF_DEFER(result);

    if (bufferString->type == AML_BUFFER)
    {
        if (aml_buffer_set_empty(result, length) == _FAIL)
        {
            return NULL;
        }
    }
    else
    {
        if (aml_string_set_empty(result, length) == _FAIL)
        {
            return NULL;
        }
    }

    uint64_t objectLength =
        bufferString->type == AML_BUFFER ? bufferString->buffer.length : bufferString->string.length;
    if (index >= objectLength)
    {
        return REF(result);
    }

    if (index + length > objectLength)
    {
        length = objectLength - index;
    }

    if (bufferString->type == AML_BUFFER)
    {
        memcpy(result->buffer.content, &bufferString->buffer.content[index], length);
    }
    else
    {
        memcpy(result->string.content, &bufferString->string.content[index], length);
        result->string.content[length] = '\0';
    }

    return REF(result);
}
