#include "buffer_field.h"

#include <errno.h>

uint64_t aml_buffer_field_load(aml_node_t* bufferField, aml_node_t* out)
{
    if (bufferField == NULL || out == NULL || bufferField->type != AML_DATA_BUFFER_FIELD)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t byteSize = (bufferField->bufferField.bitSize + 7) / 8;
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

    uint8_t* buffer = bufferField->bufferField.buffer;
    aml_bit_size_t bitOffset = bufferField->bufferField.bitOffset;
    aml_bit_size_t bitSize = bufferField->bufferField.bitSize;

    for (aml_bit_size_t i = 0; i < bitSize; i++)
    {
        aml_bit_size_t srcBitIndex = bitOffset + i;
        aml_bit_size_t srcByteIndex = srcBitIndex / 8;
        aml_bit_size_t srcBitInByte = srcBitIndex % 8;

        aml_bit_size_t destBitIndex = i;
        aml_bit_size_t destByteIndex = destBitIndex / 8;
        aml_bit_size_t destBitInByte = destBitIndex % 8;

        uint8_t bitValue = (buffer[srcByteIndex] >> srcBitInByte) & 1;

        if (out->type == AML_DATA_BUFFER)
        {
            out->buffer.content[destByteIndex] &= ~(1 << destBitInByte);
            out->buffer.content[destByteIndex] |= (bitValue << destBitInByte);
        }
        else if (out->type == AML_DATA_INTEGER)
        {
            out->integer.value &= ~((uint64_t)1 << destBitIndex);
            out->integer.value |= ((uint64_t)bitValue << destBitIndex);
        }
        else
        {
            aml_node_deinit(out);
            errno = EINVAL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_buffer_field_store(aml_node_t* bufferField, aml_node_t* in)
{
    if (bufferField == NULL || in == NULL || bufferField->type != AML_DATA_BUFFER_FIELD)
    {
        errno = EINVAL;
        return ERR;
    }

    uint8_t* buffer = bufferField->bufferField.buffer;
    aml_bit_size_t bitOffset = bufferField->bufferField.bitOffset;
    aml_bit_size_t bitSize = bufferField->bufferField.bitSize;

    for (aml_bit_size_t i = 0; i < bitSize; i++)
    {
        aml_bit_size_t destBitIndex = bitOffset + i;
        aml_bit_size_t destByteIndex = destBitIndex / 8;
        aml_bit_size_t destBitInByte = destBitIndex % 8;

        aml_bit_size_t srcBitIndex = i;
        aml_bit_size_t srcByteIndex = srcBitIndex / 8;
        aml_bit_size_t srcBitInByte = srcBitIndex % 8;

        uint8_t bitValue;
        if (in->type == AML_DATA_BUFFER)
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
        else if (in->type == AML_DATA_INTEGER)
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
