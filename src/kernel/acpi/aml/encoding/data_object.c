#include "data_object.h"

#include "acpi/aml/aml_debug.h"
#include "mem/heap.h"

#include <errno.h>

uint64_t aml_data_object_init_integer(aml_data_object_t* obj, aml_qword_data_t value, uint8_t bitWidth)
{
    if (obj == NULL || bitWidth == 0 || bitWidth > 64)
    {
        errno = EINVAL;
        return ERR;
    }

    obj->type = AML_DATA_INTEGER;
    obj->integer = value;
    obj->meta.bitWidth = bitWidth;

    return 0;
}

uint64_t aml_data_object_init_string(aml_data_object_t* obj, aml_string_t* str)
{
    if (obj == NULL || str == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    obj->type = AML_DATA_STRING;
    obj->string = *str;

    return 0;
}

uint64_t aml_data_object_init_buffer(aml_data_object_t* obj, aml_buffer_t* buffer)
{
    if (obj == NULL || buffer == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    obj->type = AML_DATA_BUFFER;
    obj->buffer = *buffer;

    return 0;
}

uint64_t aml_data_object_init_buffer_empty(aml_data_object_t* obj, uint64_t size)
{
    if (obj == NULL || size == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    uint8_t* content = heap_alloc(size, HEAP_NONE);
    if (content == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }

    obj->type = AML_DATA_BUFFER;
    obj->buffer.content = content;
    obj->buffer.length = size;
    obj->buffer.capacity = size;
    obj->buffer.allocated = true;

    return 0;
}

uint64_t aml_data_object_init_package(aml_data_object_t* obj, aml_package_t* package)
{
    if (obj == NULL || package == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    obj->type = AML_DATA_PACKAGE;
    obj->package = *package;

    return 0;
}

uint64_t aml_data_object_init_name_string(aml_data_object_t* obj, aml_name_string_t* nameString)
{
    if (obj == NULL || nameString == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    obj->type = AML_DATA_NAME_STRING;
    obj->nameString = *nameString;

    return 0;
}

uint64_t aml_data_object_init_object_reference(aml_data_object_t* obj, aml_object_reference_t* ref)
{
    if (obj == NULL || ref == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    obj->type = AML_DATA_OBJECT_REFERENCE;
    obj->objectReference = *ref;

    return 0;
}

void aml_string_deinit(aml_string_t* string)
{
    if (string->allocated && string->content != NULL)
    {
        heap_free(string->content);
        string->content = NULL;
        string->length = 0;
        string->allocated = false;
    }
}

void aml_buffer_deinit(aml_buffer_t* buffer)
{
    if (buffer->allocated && buffer->content != NULL)
    {
        heap_free(buffer->content);
        buffer->content = NULL;
        buffer->length = 0;
        buffer->capacity = 0;
        buffer->allocated = false;
    }
}

void aml_package_deinit(aml_package_t* package)
{
    if (package->elements != NULL)
    {
        for (uint64_t i = 0; i < package->numElements; i++)
        {
            aml_data_object_deinit(&package->elements[i]);
        }
        heap_free(package->elements);
        package->elements = NULL;
        package->numElements = 0;
    }
}

uint64_t aml_data_object_put_bits_at(aml_data_object_t* obj, uint64_t value, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize)
{
    if (obj == NULL || bitSize == 0 || bitSize > 64)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (obj->type)
    {
    case AML_DATA_INTEGER:
        if (bitOffset + bitSize > obj->meta.bitWidth)
        {
            errno = EINVAL;
            return ERR;
        }

        uint64_t mask;
        if (bitSize == 64)
        {
            mask = ~UINT64_C(0);
        }
        else
        {
            mask = (UINT64_C(1) << bitSize) - 1;
        }

        obj->integer &= ~(mask << bitOffset);
        obj->integer |= (value & mask) << bitOffset;
        break;
    case AML_DATA_BUFFER:
        if (bitOffset + bitSize > obj->buffer.length * 8)
        {
            errno = EINVAL;
            return ERR;
        }
        for (aml_bit_size_t i = 0; i < bitSize; i++) // TODO: Optimize
        {
            aml_bit_size_t totalBitPos = bitOffset + i;
            aml_bit_size_t bytePos = totalBitPos / 8;
            aml_bit_size_t bitPos = totalBitPos % 8;

            if (value & (UINT64_C(1) << i))
            {
                obj->buffer.content[bytePos] |= (1 << bitPos);
            }
            else
            {
                obj->buffer.content[bytePos] &= ~(1 << bitPos);
            }
        }
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

uint64_t aml_data_object_get_bits_at(aml_data_object_t* obj, aml_bit_size_t bitOffset, aml_bit_size_t bitSize,
    uint64_t* out)
{
    if (obj == NULL || out == NULL || bitSize == 0 || bitSize > 64)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (obj->type)
    {
    case AML_DATA_INTEGER:
        if (bitOffset + bitSize > obj->meta.bitWidth)
        {
            errno = EINVAL;
            return ERR;
        }

        uint64_t mask;
        if (bitSize == 64)
        {
            mask = ~UINT64_C(0);
        }
        else
        {
            mask = (UINT64_C(1) << bitSize) - 1;
        }

        *out = (obj->integer >> bitOffset) & mask;
        break;
    case AML_DATA_BUFFER:
        if (bitOffset + bitSize > obj->buffer.length * 8)
        {
            errno = EINVAL;
            return ERR;
        }
        *out = 0;
        for (aml_bit_size_t i = 0; i < bitSize; i++) // TODO: Optimize
        {
            aml_bit_size_t totalBitPos = bitOffset + i;
            aml_bit_size_t bytePos = totalBitPos / 8;
            aml_bit_size_t bitPos = totalBitPos % 8;

            if (obj->buffer.content[bytePos] & (1 << bitPos))
            {
                *out |= (UINT64_C(1) << i);
            }
        }
        break;
    case AML_DATA_STRING:
        if (bitOffset + bitSize > obj->string.length * 8)
        {
            errno = EINVAL;
            return ERR;
        }
        *out = 0;
        for (aml_bit_size_t i = 0; i < bitSize; i++) // TODO: Optimize
        {
            aml_bit_size_t totalBitPos = bitOffset + i;
            aml_bit_size_t bytePos = totalBitPos / 8;
            aml_bit_size_t bitPos = totalBitPos % 8;

            if (obj->string.content[bytePos] & (1 << bitPos))
            {
                *out |= (UINT64_C(1) << i);
            }
        }
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}
