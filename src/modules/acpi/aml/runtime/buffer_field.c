#include <kernel/acpi/aml/runtime/buffer_field.h>

#include <sys/math.h>

#include <errno.h>

#define AML_BUFFER_FIELD_TEMP_SIZE 256

uint64_t aml_buffer_field_load(aml_buffer_field_t* bufferField, aml_object_t* out)
{
    if (bufferField == NULL || out == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    uint64_t byteSize = (bufferField->bitSize + 7) / 8;
    if (byteSize > aml_integer_byte_size())
    {
        if (aml_buffer_set_empty(out, byteSize) == _FAIL)
        {
            return _FAIL;
        }
    }
    else
    {
        if (aml_integer_set(out, 0) == _FAIL)
        {
            return _FAIL;
        }
    }

    uint64_t i = 0;
    while (1)
    {
        uint64_t remainingBits = bufferField->bitSize - i;
        if (remainingBits == 0)
        {
            break;
        }

        uint8_t temp[AML_BUFFER_FIELD_TEMP_SIZE] = {0};
        uint64_t bitsToRead = MIN(remainingBits, AML_BUFFER_FIELD_TEMP_SIZE * 8);
        if (aml_object_get_bits_at(bufferField->target, bufferField->bitOffset + i, bitsToRead, temp) == _FAIL)
        {
            aml_object_clear(out);
            return _FAIL;
        }

        if (aml_object_set_bits_at(out, i, bitsToRead, temp) == _FAIL)
        {
            aml_object_clear(out);
            return _FAIL;
        }

        i += bitsToRead;
    }

    return 0;
}

uint64_t aml_buffer_field_store(aml_buffer_field_t* bufferField, aml_object_t* in)
{
    if (bufferField == NULL || in == NULL)
    {
        errno = EINVAL;
        return _FAIL;
    }

    aml_type_t inType = in->type;
    if (inType != AML_BUFFER && inType != AML_INTEGER)
    {
        errno = EINVAL;
        return _FAIL;
    }

    uint64_t i = 0;
    while (1)
    {
        uint64_t remainingBits = bufferField->bitSize - i;
        if (remainingBits == 0)
        {
            break;
        }

        uint8_t temp[AML_BUFFER_FIELD_TEMP_SIZE] = {0};
        uint64_t bitsToWrite = MIN(remainingBits, AML_BUFFER_FIELD_TEMP_SIZE * 8);
        if (aml_object_get_bits_at(in, i, bitsToWrite, temp) == _FAIL)
        {
            return _FAIL;
        }

        if (aml_object_set_bits_at(bufferField->target, bufferField->bitOffset + i, bitsToWrite, temp) == _FAIL)
        {
            return _FAIL;
        }

        i += bitsToWrite;
    }

    return 0;
}
