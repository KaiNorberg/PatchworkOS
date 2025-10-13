#include "convert.h"

#include "acpi/aml/object.h"
#include "acpi/aml/to_string.h"
#include "buffer_field.h"
#include "copy.h"
#include "field_unit.h"
#include "log/log.h"
#include "mem/heap.h"
#include "store.h"

#include <errno.h>
#include <stdio.h>

#define AML_HEX_DIGITS "0123456789ABCDEF"

#define AML_CONVERT_TRY_NEXT_CONVERTER 1

typedef uint64_t (*aml_convert_func_t)(aml_object_t* src, aml_object_t* dest);

// DebugObj handling is specified in section 19.6.26

// Note that there is a mistake in the spec in table 19.6 where it says Buffer to BufferField conversion is not allowed,
// since in table 19.7 it defines how to do it and the ACPICA tests clearly expect it to work, we implement it as
// specified in table 19.7.

typedef struct
{
    aml_type_t srcType;
    aml_type_t destType;
    aml_convert_func_t convertFunc;
} aml_convert_entry_t;

static inline uint64_t aml_string_resize(aml_string_obj_t* string, uint64_t newLength)
{
    if (string == NULL || newLength == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (newLength == string->length)
    {
        return 0;
    }

    char* newContent = heap_alloc(newLength + 1, HEAP_NONE);
    if (newContent == NULL)
    {
        return ERR;
    }

    uint64_t copyLen = MIN(newLength, string->length);
    for (uint64_t i = 0; i < copyLen; i++)
    {
        newContent[i] = string->content[i];
    }
    for (uint64_t i = copyLen; i < newLength; i++)
    {
        newContent[i] = '\0';
    }
    newContent[newLength] = '\0';

    if (string->content != NULL)
    {
        heap_free(string->content);
    }
    string->content = newContent;
    string->length = newLength;
    return 0;
}

static inline uint64_t aml_string_prepare(aml_object_t* obj, uint64_t length)
{
    aml_type_t type = obj->type;
    if (type == AML_STRING)
    {
        if (aml_string_resize(&obj->string, length) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_string_set_empty(obj, length) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}

static inline void aml_byte_to_hex(uint8_t byte, char* dest)
{
    dest[0] = AML_HEX_DIGITS[byte >> 4];
    dest[1] = AML_HEX_DIGITS[byte & 0x0F];
}

static inline uint8_t aml_hex_to_byte(char chr)
{
    if (chr >= '0' && chr <= '9')
        return chr - '0';
    if (chr >= 'a' && chr <= 'f')
        return chr - 'a' + 10;
    if (chr >= 'A' && chr <= 'F')
        return chr - 'A' + 10;
    return 16;
}

// We create the arrays of converters here. The order of the list defines the priority of the converters. First ==
// Highest Priority. Last == Lowest Priority. See section 19.3.5.7 table 19.6 for the conversion priority order.

static uint64_t aml_buffer_to_buffer_field(aml_object_t* buffer, aml_object_t* dest)
{
    if (dest->type != AML_BUFFER_FIELD)
    {
        return AML_CONVERT_TRY_NEXT_CONVERTER;
    }
    return aml_buffer_field_store(&dest->bufferField, buffer);
}

static uint64_t aml_buffer_to_integer(aml_object_t* buffer, aml_object_t* dest)
{
    aml_buffer_obj_t* bufferData = &buffer->buffer;

    uint64_t value = 0;
    uint64_t maxBytes = MIN(bufferData->length, aml_integer_byte_size());
    for (uint64_t i = 0; i < maxBytes; i++)
    {
        value |= ((uint64_t)bufferData->content[i]) << (i * 8);
    }

    if (dest->type == AML_INTEGER)
    {
        dest->integer.value = value;
        return 0;
    }
    return aml_integer_set(dest, value);
}

static uint64_t aml_buffer_to_string(aml_object_t* buffer, aml_object_t* dest)
{
    aml_buffer_obj_t* bufferData = &buffer->buffer;
    // Each byte becomes two hex chars with a space in between, except the last byte.
    uint64_t length = bufferData->length > 0 ? bufferData->length * 3 - 1 : 0;

    if (aml_string_prepare(dest, length) == ERR)
    {
        return ERR;
    }

    char* content = dest->string.content;
    for (uint64_t i = 0; i < bufferData->length; i++)
    {
        aml_byte_to_hex(bufferData->content[i], &content[i * 3]);
        if (i < bufferData->length - 1)
        {
            content[i * 3 + 2] = ' ';
        }
    }

    return 0;
}

static uint64_t aml_buffer_to_debug_object(aml_object_t* buffer, aml_object_t* dest)
{
    (void)dest;

    if (buffer->buffer.length == 0)
    {
        LOG_INFO("[]\n");
        return 0;
    }

    LOG_INFO("[");
    for (uint64_t i = 0; i < buffer->buffer.length; i++)
    {
        LOG_INFO("%02X", buffer->buffer.content[i]);
        if (i < buffer->buffer.length - 1)
        {
            LOG_INFO(", ");
        }
    }
    LOG_INFO("]\n");
    return 0;
}

static aml_convert_entry_t bufferConverters[AML_TYPE_AMOUNT] = {
    {AML_BUFFER, AML_BUFFER_FIELD, aml_buffer_to_buffer_field},
    {AML_BUFFER, AML_INTEGER, aml_buffer_to_integer},
    {AML_BUFFER, AML_STRING, aml_buffer_to_string},
    {AML_BUFFER, AML_DEBUG_OBJECT, aml_buffer_to_debug_object},
};

static uint64_t aml_integer_to_buffer(aml_object_t* integer, aml_object_t* dest)
{
    aml_integer_obj_t* integerData = &integer->integer;

    if (dest->type == AML_BUFFER)
    {
        uint64_t length = MIN(aml_integer_byte_size(), dest->buffer.length);
        uint8_t* content = dest->buffer.content;

        for (uint64_t i = 0; i < length; i++)
        {
            content[i] = (integerData->value >> (i * 8)) & 0xFF;
        }
        for (uint64_t i = aml_integer_byte_size(); i < dest->buffer.length; i++)
        {
            content[i] = 0;
        }
        return 0;
    }

    return aml_buffer_set(dest, (uint8_t*)&integerData->value, aml_integer_byte_size(), aml_integer_byte_size());
}

static uint64_t aml_integer_to_field_unit(aml_object_t* integer, aml_object_t* dest)
{
    if (dest->type != AML_FIELD_UNIT)
    {
        return AML_CONVERT_TRY_NEXT_CONVERTER;
    }
    return aml_field_unit_store(&dest->fieldUnit, integer);
}

static uint64_t aml_integer_to_buffer_field(aml_object_t* integer, aml_object_t* dest)
{
    if (dest->type != AML_BUFFER_FIELD)
    {
        return AML_CONVERT_TRY_NEXT_CONVERTER;
    }
    return aml_buffer_field_store(&dest->bufferField, integer);
}

static uint64_t aml_integer_to_string(aml_object_t* integer, aml_object_t* dest)
{
    const uint64_t stringLength = aml_integer_byte_size() * 2; // Two hex chars per byte

    if (aml_string_prepare(dest, stringLength) == ERR)
    {
        return ERR;
    }

    aml_integer_obj_t* integerData = &integer->integer;

    char* content = dest->string.content;
    for (uint64_t i = 0; i < aml_integer_byte_size(); i++)
    {
        uint8_t byte = (integerData->value >> (i * 8)) & 0xFF;
        aml_byte_to_hex(byte, &content[aml_integer_byte_size() * 2 - i * 2 - 2]);
    }

    return 0;
}

static uint64_t aml_integer_to_debug_object(aml_object_t* integer, aml_object_t* dest)
{
    (void)dest;
    LOG_INFO("0x%llx\n", integer->integer.value);
    return 0;
}

static aml_convert_entry_t integerConverters[AML_TYPE_AMOUNT] = {
    {AML_INTEGER, AML_BUFFER, aml_integer_to_buffer},
    {AML_INTEGER, AML_BUFFER_FIELD, aml_integer_to_buffer_field},
    {AML_INTEGER, AML_FIELD_UNIT, aml_integer_to_field_unit},
    {AML_INTEGER, AML_STRING, aml_integer_to_string},
    {AML_INTEGER, AML_DEBUG_OBJECT, aml_integer_to_debug_object},
};

static uint64_t aml_package_to_debug_object(aml_object_t* package, aml_object_t* dest)
{
    (void)dest;
    LOG_INFO("[");
    for (uint64_t i = 0; i < package->package.length; i++)
    {
        LOG_INFO("%s", aml_object_to_string(package->package.elements[i]));
        if (i < package->package.length - 1)
        {
            LOG_INFO(", ");
        }
    }
    LOG_INFO("]\n");
    return 0;
}

static aml_convert_entry_t packageConverters[AML_TYPE_AMOUNT] = {
    {AML_PACKAGE, AML_DEBUG_OBJECT, aml_package_to_debug_object},
};

static uint64_t aml_string_to_integer(aml_object_t* string, aml_object_t* dest)
{
    aml_string_obj_t* stringData = &string->string;

    uint64_t value = 0;
    uint64_t maxChars = MIN(stringData->length, aml_integer_byte_size() * 2); // Two hex chars per byte

    for (uint64_t i = 0; i < maxChars; i++)
    {
        uint8_t digit = aml_hex_to_byte(stringData->content[i]);
        if (digit == 16) // Stop at first non-hex character
        {
            break;
        }
        value = value * 16 + digit;
    }

    if (dest->type == AML_INTEGER)
    {
        dest->integer.value = value;
        return 0;
    }
    return aml_integer_set(dest, value);
}

static uint64_t aml_string_to_buffer(aml_object_t* string, aml_object_t* dest)
{
    aml_string_obj_t* stringData = &string->string;

    uint64_t bufferLength = 0;

    if (dest->type == AML_BUFFER)
    {
        bufferLength = dest->buffer.length;
    }
    else
    {
        bufferLength = stringData->length > 0 ? stringData->length + 1 : 0;
        if (aml_buffer_set_empty(dest, bufferLength) == ERR)
        {
            return ERR;
        }
    }

    if (bufferLength > 0)
    {
        uint8_t* content = dest->buffer.content;
        uint64_t copyLen = MIN(stringData->length, bufferLength - 1);

        for (uint64_t i = 0; i < copyLen; i++)
        {
            content[i] = (uint8_t)stringData->content[i];
        }
        content[bufferLength - 1] = 0;
    }

    return 0;
}

static uint64_t aml_string_to_debug_object(aml_object_t* string, aml_object_t* dest)
{
    (void)dest;
    if (string->string.length == 0)
    {
        LOG_INFO("\"\"\n");
        return 0;
    }

    LOG_INFO("\"%s\"\n", string->string.content);
    return 0;
}

static aml_convert_entry_t stringConverters[AML_TYPE_AMOUNT] = {
    {AML_STRING, AML_INTEGER, aml_string_to_integer},
    {AML_STRING, AML_BUFFER, aml_string_to_buffer},
    {AML_STRING, AML_DEBUG_OBJECT, aml_string_to_debug_object},
};

static aml_convert_entry_t* aml_converters_get(aml_type_t srcType)
{
    switch (srcType)
    {
    case AML_BUFFER:
        return bufferConverters;
    case AML_INTEGER:
        return integerConverters;
    case AML_PACKAGE:
        return packageConverters;
    case AML_STRING:
        return stringConverters;
    case AML_DEBUG_OBJECT:
        errno = ENOSYS;
        return NULL;
    default:
        errno = EINVAL;
        return NULL;
    }
}

uint64_t aml_convert(aml_object_t* src, aml_object_t* dest, aml_type_t allowedTypes)
{
    if (dest == NULL || src == NULL)
    {
        LOG_ERR("src/dest object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_UNINITIALIZED)
    {
        LOG_ERR("source object is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    // BufferFields and FieldUnits are treated as either Buffers or Integers based on size
    if (src->type == AML_FIELD_UNIT || src->type == AML_BUFFER_FIELD)
    {
        aml_object_t* temp = aml_object_new();
        if (temp == NULL)
        {
            return ERR;
        }
        DEREF_DEFER(temp);

        if (src->type == AML_FIELD_UNIT)
        {
            if (aml_field_unit_load(&src->fieldUnit, temp) == ERR)
            {
                LOG_ERR("failed to load FieldUnit\n");
                return ERR;
            }
        }
        else // AML_BUFFER_FIELD
        {
            if (aml_buffer_field_load(&src->bufferField, temp) == ERR)
            {
                LOG_ERR("failed to load BufferField\n");
                return ERR;
            }
        }

        aml_type_t tempType = temp->type;
        if (tempType & allowedTypes)
        {
            if (aml_copy_data_and_type(temp, dest) == ERR)
            {
                LOG_ERR("failed to copy loaded field to destination\n");
                return ERR;
            }
            return 0;
        }

        return aml_convert(temp, dest, allowedTypes);
    }

    // AML seems to prioritize copying over conversion if the types match even if its not the
    // highest priority conversion.
    if (src->type & allowedTypes && (src->type == dest->type || dest->type == AML_UNINITIALIZED))
    {
        if (aml_copy_data_and_type(src, dest) == ERR)
        {
            LOG_ERR("failed to copy from '%s' to '%s'\n", aml_type_to_string(src->type),
                aml_type_to_string(dest->type));
            return ERR;
        }
        return 0;
    }

    aml_convert_entry_t* converters = aml_converters_get(src->type);
    if (converters == NULL)
    {
        LOG_ERR("no converters defined for source type '%s'\n", aml_type_to_string(src->type));
        return ERR;
    }

    for (size_t i = 0; i < AML_TYPE_AMOUNT; i++)
    {
        aml_convert_entry_t* entry = &converters[i];

        if (entry->srcType == 0) // We reached the end of the array
        {
            LOG_ERR("no valid converter found from '%s' to any allowed type\n", aml_type_to_string(src->type));
            errno = EILSEQ;
            return ERR;
        }

        if (!(allowedTypes & entry->destType))
        {
            continue;
        }

        if (entry->convertFunc == NULL)
        {
            LOG_ERR("converter from '%s' to '%s' not implemented\n", aml_type_to_string(src->type),
                aml_type_to_string(entry->destType));
            errno = ENOSYS;
            return ERR;
        }

        uint64_t result = entry->convertFunc(src, dest);
        if (result == ERR)
        {
            LOG_ERR("conversion from '%s' to '%s' failed\n", aml_type_to_string(src->type),
                aml_type_to_string(entry->destType));
            return ERR;
        }
        else if (result == AML_CONVERT_TRY_NEXT_CONVERTER)
        {
            continue;
        }

        return 0;
    }

    errno = EILSEQ;
    return ERR;
}

uint64_t aml_convert_result(aml_object_t* result, aml_object_t* target)
{
    if (result == NULL)
    {
        LOG_ERR("result object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (result->type == AML_UNINITIALIZED || target->type == AML_UNINITIALIZED)
    {
        LOG_ERR("result/target object is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    if (target == NULL)
    {
        return 0;
    }

    if (target->type == AML_ARG || target->type == AML_LOCAL)
    {
        if (aml_store(result, target) == ERR)
        {
            LOG_ERR("failed to copy result '%s' to target local/arg\n", aml_type_to_string(result->type));
            return ERR;
        }
        return 0;
    }

    // I am assuming that "fixed type" means not a DataRefObject, let me know if this is wrong.
    if (!(target->type & AML_DATA_REF_OBJECTS))
    {
        if (aml_convert(result, target, target->type) == ERR)
        {
            LOG_ERR("failed to convert result '%s' to target '%s'\n", aml_type_to_string(result->type),
                aml_type_to_string(target->type));
            return ERR;
        }
        return 0;
    }

    if (aml_copy_data_and_type(result, target) == ERR)
    {
        LOG_ERR("failed to copy result '%s' to target DataRefObject\n", aml_type_to_string(result->type));
        return ERR;
    }

    return 0;
}

uint64_t aml_convert_source(aml_object_t* src, aml_object_t** dest, aml_type_t allowedTypes)
{
    if (src == NULL || dest == NULL)
    {
        LOG_ERR("src/dest object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_UNINITIALIZED)
    {
        LOG_ERR("source object is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_ARG)
    {
        return aml_convert_source(src->arg.value, dest, allowedTypes);
    }

    if (src->type == AML_LOCAL)
    {
        return aml_convert_source(src->local.value, dest, allowedTypes);
    }

    if (src->type & allowedTypes)
    {
        if (*dest != NULL)
        {
            return aml_copy_data_and_type(src, *dest);
        }
        *dest = REF(src);
        return 0;
    }

    if (*dest != NULL)
    {
        return aml_convert(src, *dest, allowedTypes);
    }

    *dest = aml_object_new();
    if (*dest == NULL)
    {
        return ERR;
    }

    if (aml_convert(src, *dest, allowedTypes) == ERR)
    {
        DEREF(*dest);
        *dest = NULL;
        return ERR;
    }

    return 0;
}

uint64_t aml_convert_to_buffer(aml_object_t* src, aml_object_t** dest)
{
    if (src == NULL || dest == NULL)
    {
        LOG_ERR("src/dest object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_UNINITIALIZED)
    {
        LOG_ERR("src object is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_BUFFER)
    {
        *dest = REF(src);
        return 0;
    }

    aml_object_t* temp = aml_object_new();
    if (temp == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(temp);

    if (src->type == AML_INTEGER)
    {
        if (aml_integer_to_buffer(src, temp) == ERR)
        {
            return ERR;
        }
        *dest = REF(temp);
        return 0;
    }
    else if (src->type == AML_STRING)
    {
        if (aml_string_to_buffer(src, temp) == ERR)
        {
            return ERR;
        }
        *dest = REF(temp);
        return 0;
    }

    LOG_ERR("cannot convert '%s' to Buffer\n", aml_type_to_string(src->type));
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_convert_to_decimal_string(aml_object_t* src, aml_object_t** dest)
{
    if (src == NULL || dest == NULL)
    {
        LOG_ERR("src/dest object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_UNINITIALIZED)
    {
        LOG_ERR("src object is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_STRING)
    {
        *dest = REF(src);
        return 0;
    }

    aml_object_t* temp = aml_object_new();
    if (temp == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(temp);

    if (src->type == AML_INTEGER)
    {
        char buffer[21]; // Max uint64_t is 20 digits + null terminator
        int length = snprintf(buffer, sizeof(buffer), "%llu", src->integer.value);
        if (length < 0)
        {
            return ERR;
        }

        if (aml_string_set_empty(temp, length) == ERR)
        {
            return ERR;
        }
        memcpy(temp->string.content, buffer, length);
        *dest = REF(temp);
        return 0;
    }
    else if (src->type == AML_BUFFER)
    {
        aml_buffer_obj_t* bufferData = &src->buffer;

        if (bufferData->length == 0)
        {
            if (aml_string_set_empty(temp, 0) == ERR)
            {
                return ERR;
            }
            *dest = REF(temp);
            return 0;
        }

        uint64_t maxLen = bufferData->length * 4; // "255," per byte worst case
        char* buff = heap_alloc(maxLen + 1, HEAP_NONE);
        if (buff == NULL)
        {
            return ERR;
        }

        char* p = buff;
        char* end = p + maxLen;
        for (uint64_t i = 0; i < bufferData->length; i++)
        {
            int written = snprintf(p, end - p, "%u", bufferData->content[i]);
            if (written < 0 || p + written >= end)
            {
                heap_free(buff);
                errno = EILSEQ;
                return ERR;
            }
            p += written;

            if (i < bufferData->length - 1 && p < end - 1)
            {
                *p++ = ',';
            }
        }
        *p = '\0';

        uint64_t length = p - buff;
        if (aml_string_prepare(temp, length) == ERR)
        {
            heap_free(temp);
            return ERR;
        }
        memcpy(temp->string.content, temp, length);
        heap_free(buff);
        *dest = REF(temp);
        return 0;
    }

    LOG_ERR("cannot convert '%s' to String\n", aml_type_to_string(src->type));
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_convert_to_hex_string(aml_object_t* src, aml_object_t** dest)
{
    if (src == NULL || dest == NULL)
    {
        LOG_ERR("src/dest object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_UNINITIALIZED)
    {
        LOG_ERR("src object is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_STRING)
    {
        *dest = REF(src);
        return 0;
    }

    aml_object_t* temp = aml_object_new();
    if (temp == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(temp);

    if (src->type == AML_INTEGER)
    {
        char buffer[17]; // 16 hex digits + null terminator
        int len = snprintf(buffer, sizeof(buffer), "%llx", (unsigned long long)src->integer.value);
        if (len < 0)
        {
            return ERR;
        }

        if (aml_string_set_empty(temp, len) == ERR)
        {
            DEREF(temp);
            return ERR;
        }
        memcpy(temp->string.content, buffer, len + 1);
        *dest = REF(temp);
        return 0;
    }
    else if (src->type == AML_BUFFER)
    {
        aml_buffer_obj_t* bufferData = &src->buffer;

        if (bufferData->length == 0)
        {
            if (aml_string_set_empty(temp, 0) == ERR)
            {
                DEREF(temp);
                return ERR;
            }
            *dest = REF(temp);
            return 0;
        }

        uint64_t len = bufferData->length * 3 - 1; // "XX," per byte except last
        if (aml_string_set_empty(temp, len) == ERR)
        {
            DEREF(temp);
            return ERR;
        }

        char* content = temp->string.content;
        for (uint64_t i = 0; i < bufferData->length; i++)
        {
            aml_byte_to_hex(bufferData->content[i], &content[i * 3]);
            if (i < bufferData->length - 1)
            {
                content[i * 3 + 2] = ',';
            }
        }
        content[len] = '\0';
        *dest = REF(temp);
        return 0;
    }

    LOG_ERR("cannot convert '%s' to String\n", aml_type_to_string(src->type));
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_convert_to_integer(aml_object_t* src, aml_object_t** dest)
{
    if (src == NULL || dest == NULL)
    {
        LOG_ERR("src/dest object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_UNINITIALIZED)
    {
        LOG_ERR("src object is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_INTEGER)
    {
        *dest = REF(src);
        return 0;
    }

    aml_object_t* temp = aml_object_new();
    if (temp == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(temp);

    if (src->type == AML_STRING)
    {
        aml_string_obj_t* stringData = &src->string;
        if (stringData->length == 0 || stringData->content == NULL)
        {
            errno = EILSEQ;
            return ERR;
        }

        // Determine if hex (0x prefix) or decimal
        bool isHex = false;
        uint64_t i = 0;
        if (stringData->length > 2 && stringData->content[0] == '0' &&
            (stringData->content[1] == 'x' || stringData->content[1] == 'X'))
        {
            isHex = true;
            i = 2;
        }

        // "If the value exceeds the maximum, the result is unpredictable" - ACPI Spec
        aml_integer_t value = 0;
        for (; i < stringData->length; i++)
        {
            char chr = stringData->content[i];

            if (isHex)
            {
                uint8_t digit = aml_hex_to_byte(chr);
                if (digit == 16) // Stop at first non-hex character
                {
                    break;
                }
                value = value * 16 + digit;
            }
            else
            {
                if (chr >= '0' && chr <= '9')
                {
                    value = value * 10 + (chr - '0');
                }
                else
                {
                    break;
                }
            }
        }

        if (aml_integer_set(temp, value) == ERR)
        {
            return ERR;
        }
        *dest = REF(temp);
        return 0;
    }
    else if (src->type == AML_BUFFER)
    {
        if (aml_buffer_to_integer(src, temp) == ERR)
        {
            return ERR;
        }
        *dest = REF(temp);
        return 0;
    }

    LOG_ERR("cannot convert '%s' to Integer\n", aml_type_to_string(src->type));
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_convert_integer_to_bcd(aml_integer_t value, aml_integer_t* out)
{
    if (out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_integer_t bcd = 0;
    for (uint64_t i = 0; i < aml_integer_byte_size() * 2; i++) // 2 nibbles per byte
    {
        uint8_t digit = value % 10;
        bcd |= ((aml_integer_t)digit) << (i * 4);
        value /= 10;
        if (value == 0)
            break;
    }

    *out = bcd;
    return 0;
}
