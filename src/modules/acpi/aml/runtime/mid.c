#include <kernel/acpi/aml/runtime/mid.h>

status_t aml_mid(aml_state_t* state, aml_object_t* bufferString, aml_uint_t index, aml_uint_t length, aml_object_t** out)
{
    if (state == NULL || bufferString == NULL || (bufferString->type != AML_BUFFER && bufferString->type != AML_STRING))
    {
        return ERR(ACPI, INVAL);
    }

    aml_object_t* result = aml_object_new();
    if (result == NULL)
    {
        return ERR(ACPI, NOMEM);
    }
    UNREF_DEFER(result);

    if (bufferString->type == AML_BUFFER)
    {
        status_t status = aml_buffer_set_empty(result, length);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    else
    {
        status_t status = aml_string_set_empty(result, length);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    uint64_t objectLength =
        bufferString->type == AML_BUFFER ? bufferString->buffer.length : bufferString->string.length;
    if (index >= objectLength)
    {
        *out = REF(result);
        return OK;
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

    *out = REF(result);
    return OK;
}
