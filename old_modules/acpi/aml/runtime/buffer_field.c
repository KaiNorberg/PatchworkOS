#include <kernel/acpi/aml/runtime/buffer_field.h>

#include <sys/math.h>

#include <sys/status.h>

#define AML_BUFFER_FIELD_TEMP_SIZE 256

status_t aml_buffer_field_load(aml_buffer_field_t* bufferField, aml_object_t* out)
{
    if (bufferField == NULL || out == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    uint64_t byteSize = (bufferField->bitSize + 7) / 8;
    if (byteSize > aml_integer_byte_size())
    {
        status_t status = aml_buffer_set_empty(out, byteSize);
        if (IS_ERR(status))
        {
            return status;
        }
    }
    else
    {
        status_t status = aml_integer_set(out, 0);
        if (IS_ERR(status))
        {
            return status;
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
        status_t status = aml_object_get_bits_at(bufferField->target, bufferField->bitOffset + i, bitsToRead, temp);
        if (IS_ERR(status))
        {
            aml_object_clear(out);
            return status;
        }

        status = aml_object_set_bits_at(out, i, bitsToRead, temp);
        if (IS_ERR(status))
        {
            aml_object_clear(out);
            return status;
        }

        i += bitsToRead;
    }

    return OK;
}

status_t aml_buffer_field_store(aml_buffer_field_t* bufferField, aml_object_t* in)
{
    if (bufferField == NULL || in == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    aml_type_t inType = in->type;
    if (inType != AML_BUFFER && inType != AML_INTEGER)
    {
        return ERR(ACPI, INVAL);
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
        status_t status = aml_object_get_bits_at(in, i, bitsToWrite, temp);
        if (IS_ERR(status))
        {
            return status;
        }

        status = aml_object_set_bits_at(bufferField->target, bufferField->bitOffset + i, bitsToWrite, temp);
        if (IS_ERR(status))
        {
            return status;
        }

        i += bitsToWrite;
    }

    return OK;
}
