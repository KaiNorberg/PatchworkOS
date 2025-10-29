#include <kernel/acpi/aml/runtime/buffer_field.h>

#include <sys/math.h>

#include <errno.h>

#define AML_BUFFER_FIELD_TEMP_SIZE 256

uint64_t aml_buffer_field_load(aml_buffer_field_obj_t* bufferField, aml_object_t* out)
{
    if (bufferField == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t byteSize = (bufferField->bitSize + 7) / 8;
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
        if (aml_object_get_bits_at(bufferField->target, bufferField->bitOffset + i, bitsToRead, temp) == ERR)
        {
            aml_object_clear(out);
            return ERR;
        }

        if (aml_object_set_bits_at(out, i, bitsToRead, temp) == ERR)
        {
            aml_object_clear(out);
            return ERR;
        }

        i += bitsToRead;
    }

    return 0;
}

uint64_t aml_buffer_field_store(aml_buffer_field_obj_t* bufferField, aml_object_t* in)
{
    if (bufferField == NULL || in == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_type_t inType = in->type;
    if (inType != AML_BUFFER && inType != AML_INTEGER)
    {
        errno = EINVAL;
        return ERR;
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
        if (aml_object_get_bits_at(in, i, bitsToWrite, temp) == ERR)
        {
            return ERR;
        }

        if (aml_object_set_bits_at(bufferField->target, bufferField->bitOffset + i, bitsToWrite, temp) == ERR)
        {
            return ERR;
        }

        i += bitsToWrite;
    }

    return 0;
}
