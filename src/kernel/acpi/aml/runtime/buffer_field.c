#include "buffer_field.h"

#include <errno.h>

uint64_t aml_buffer_field_load(aml_buffer_field_t* bufferField, aml_object_t* out)
{
    if (bufferField == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint8_t* buffer;
    if (bufferField->isString)
    {
        buffer = (uint8_t*)bufferField->string->content;
    }
    else
    {
        buffer = bufferField->buffer->content;
    }
    aml_bit_size_t bitOffset = bufferField->bitOffset;
    aml_bit_size_t bitSize = bufferField->bitSize;

    uint64_t byteSize = (bitSize + 7) / 8;
    if (byteSize > sizeof(uint64_t))
    {
        if (aml_buffer_init_empty(out, byteSize) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_integer_init(out, 0) == ERR)
        {
            return ERR;
        }
    }

    if (bitOffset + bitSize >
        (bufferField->isString ? bufferField->string->length * 8 : bufferField->buffer->length * 8))
    {
        errno = EINVAL;
        return ERR;
    }

    for (aml_bit_size_t i = 0; i < bitSize; i++)
    {
        aml_bit_size_t srcBitIndex = bitOffset + i;
        aml_bit_size_t srcByteIndex = srcBitIndex / 8;
        aml_bit_size_t srcBitInByte = srcBitIndex % 8;

        aml_bit_size_t destBitIndex = i;
        aml_bit_size_t destByteIndex = destBitIndex / 8;
        aml_bit_size_t destBitInByte = destBitIndex % 8;

        uint8_t bitValue = (buffer[srcByteIndex] >> srcBitInByte) & 1;

        if (byteSize > sizeof(uint64_t))
        {
            out->buffer.content[destByteIndex] &= ~(1 << destBitInByte);
            out->buffer.content[destByteIndex] |= (bitValue << destBitInByte);
        }
        else
        {
            out->integer.value &= ~((uint64_t)1 << destBitIndex);
            out->integer.value |= ((uint64_t)bitValue << destBitIndex);
        }
    }

    return 0;
}

uint64_t aml_buffer_field_store(aml_buffer_field_t* bufferField, aml_object_t* in)
{
    if (bufferField == NULL || in == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint8_t* buffer;
    if (bufferField->isString)
    {
        buffer = (uint8_t*)bufferField->string->content;
    }
    else
    {
        buffer = bufferField->buffer->content;
    }
    aml_bit_size_t bitOffset = bufferField->bitOffset;
    aml_bit_size_t bitSize = bufferField->bitSize;

    aml_type_t inType = in->type;
    if (inType != AML_BUFFER && inType != AML_INTEGER)
    {
        errno = EINVAL;
        return ERR;
    }

    if (bitOffset + bitSize >
        (bufferField->isString ? bufferField->string->length * 8 : bufferField->buffer->length * 8))
    {
        errno = EINVAL;
        return ERR;
    }

    for (aml_bit_size_t i = 0; i < bitSize; i++)
    {
        aml_bit_size_t destBitIndex = bitOffset + i;
        aml_bit_size_t destByteIndex = destBitIndex / 8;
        aml_bit_size_t destBitInByte = destBitIndex % 8;

        aml_bit_size_t srcBitIndex = i;
        aml_bit_size_t srcByteIndex = srcBitIndex / 8;
        aml_bit_size_t srcBitInByte = srcBitIndex % 8;

        uint8_t bitValue;
        if (inType == AML_BUFFER)
        {
            if (srcByteIndex >= in->buffer.length)
            {
                bitValue = 0;
            }
            else
            {
                bitValue = (in->buffer.content[srcByteIndex] >> srcBitInByte) & 1;
            }
        }
        else if (inType == AML_INTEGER)
        {
            if (srcBitIndex >= 64)
            {
                bitValue = 0;
            }
            else
            {
                bitValue = (in->integer.value >> srcBitIndex) & 1;
            }
        }
        else
        {
            errno = EINVAL;
            return ERR;
        }

        buffer[destByteIndex] &= ~(1 << destBitInByte);
        buffer[destByteIndex] |= (bitValue << destBitInByte);
    }

    return 0;
}
