#include "convert.h"

#include "acpi/aml/aml_object.h"
#include "acpi/aml/aml_to_string.h"
#include "buffer_field.h"
#include "copy.h"
#include "log/log.h"
#include "opregion.h"

#include <errno.h>

#define AML_CONVERT_TRY_NEXT_CONVERTER 1

typedef uint64_t (*aml_convert_func_t)(aml_object_t* src, aml_object_t* dest);

typedef struct
{
    aml_data_type_t srcType;
    aml_data_type_t destType;
    aml_convert_func_t convertFunc;
} aml_convert_entry_t;

// We create the arrays of converters here. The order of the list defines the priority of the converters. First ==
// Highest Priority. Last == Lowest Priority. See section 19.3.5.7 table 19.6 for the conversion priority order.

static uint64_t aml_buffer_to_integer(aml_object_t* buffer, aml_object_t* dest)
{
    uint64_t value = 0;
    for (uint64_t i = 0; i < buffer->buffer.length && i < sizeof(uint64_t); i++)
    {
        value |= ((uint64_t)buffer->buffer.content[i]) << (i * 8);
    }

    if (aml_object_init_integer(dest, value) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_buffer_to_string(aml_object_t* buffer, aml_object_t* dest)
{
    // Each byte in the buffer becomes two hex chars with a space in between.
    uint64_t length = buffer->buffer.length * 2 + (buffer->buffer.length > 0 ? buffer->buffer.length - 1 : 0);

    // Will add the null terminator on its own.
    if (aml_object_init_string_empty(dest, length) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < buffer->buffer.length; i++)
    {
        // Honestly just did it this way becouse it looks cool.
        uint8_t byte = buffer->buffer.content[i];
        dest->string.content[i * 3 + 0] = "0123456789ABCDEF"[byte >> 4];
        dest->string.content[i * 3 + 1] = "0123456789ABCDEF"[byte & 0x0F];
        if (i < buffer->buffer.length - 1)
        {
            dest->string.content[i * 3 + 2] = ' ';
        }
    }

    return 0;
}

static uint64_t aml_buffer_to_debug_object(aml_object_t* buffer, aml_object_t* dest)
{
    (void)buffer;
    (void)dest;

    // Debug objects are unimplemented.
    errno = ENOSYS;
    return ERR;
}

static aml_convert_entry_t bufferConverters[AML_DATA_TYPE_AMOUNT] = {
    {AML_DATA_BUFFER, AML_DATA_INTEGER, aml_buffer_to_integer},
    {AML_DATA_BUFFER, AML_DATA_STRING, aml_buffer_to_string},
    {AML_DATA_BUFFER, AML_DATA_DEBUG_OBJECT, aml_buffer_to_debug_object},
};

static uint64_t aml_integer_to_buffer(aml_object_t* integer, aml_object_t* dest)
{
    if (dest->type == AML_DATA_BUFFER)
    {
        for (uint64_t i = 0; i < dest->buffer.length && i < sizeof(uint64_t); i++)
        {
            dest->buffer.content[i] = (integer->integer.value >> (i * 8)) & 0xFF;
        }
        for (uint64_t i = sizeof(uint64_t); i < dest->buffer.length; i++)
        {
            dest->buffer.content[i] = 0;
        }
    }

    if (aml_object_init_buffer(dest, (uint8_t*)&integer->integer.value, sizeof(uint64_t), sizeof(uint64_t)) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_integer_to_field_unit(aml_object_t* integer, aml_object_t* dest)
{
    if (dest->type != AML_DATA_FIELD_UNIT)
    {
        return AML_CONVERT_TRY_NEXT_CONVERTER;
    }

    if (aml_field_unit_store(dest, integer) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_integer_to_buffer_field(aml_object_t* integer, aml_object_t* dest)
{
    if (dest->type != AML_DATA_BUFFER_FIELD)
    {
        return AML_CONVERT_TRY_NEXT_CONVERTER;
    }

    if (aml_buffer_field_store(dest, integer) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_integer_to_string(aml_object_t* integer, aml_object_t* dest)
{
    uint64_t stringLength = sizeof(uint64_t) * 2; // Each byte becomes two hex chars.

    if (aml_object_init_string_empty(dest, stringLength) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < sizeof(uint64_t); i++)
    {
        uint8_t byte = (integer->integer.value >> (i * 8)) & 0xFF;
        dest->string.content[i * 2 + 0] = "0123456789ABCDEF"[byte >> 4];
        dest->string.content[i * 2 + 1] = "0123456789ABCDEF"[byte & 0x0F];
    }

    return 0;
}

static uint64_t aml_integer_to_debug_object(aml_object_t* integer, aml_object_t* dest)
{
    (void)integer;
    (void)dest;

    // Debug objects are unimplemented.
    errno = ENOSYS;
    return ERR;
}

static aml_convert_entry_t integerConverters[AML_DATA_TYPE_AMOUNT] = {
    {AML_DATA_INTEGER, AML_DATA_BUFFER, aml_integer_to_buffer},
    {AML_DATA_INTEGER, AML_DATA_BUFFER_FIELD, aml_integer_to_buffer_field},
    {AML_DATA_INTEGER, AML_DATA_FIELD_UNIT, aml_integer_to_field_unit},
    {AML_DATA_INTEGER, AML_DATA_STRING, aml_integer_to_string},
    {AML_DATA_INTEGER, AML_DATA_DEBUG_OBJECT, aml_integer_to_debug_object},
};

static uint64_t aml_integer_constant_to_integer(aml_object_t* integerConstant, aml_object_t* dest)
{
    if (aml_object_init_integer(dest, integerConstant->integerConstant.value) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_integer_constant_to_debug_object(aml_object_t* integerConstant, aml_object_t* dest)
{
    (void)integerConstant;
    (void)dest;

    // Debug objects are unimplemented.
    errno = ENOSYS;
    return ERR;
}

static aml_convert_entry_t integerConstantConverters[AML_DATA_TYPE_AMOUNT] = {
    {AML_DATA_INTEGER_CONSTANT, AML_DATA_INTEGER, aml_integer_constant_to_integer},
    {AML_DATA_INTEGER_CONSTANT, AML_DATA_DEBUG_OBJECT, aml_integer_constant_to_debug_object},
};

static uint64_t aml_package_to_debug_object(aml_object_t* package, aml_object_t* dest)
{
    (void)package;
    (void)dest;

    // Debug objects are unimplemented.
    errno = ENOSYS;
    return ERR;
}

static aml_convert_entry_t packageConverters[AML_DATA_TYPE_AMOUNT] = {
    {AML_DATA_PACKAGE, AML_DATA_DEBUG_OBJECT, aml_package_to_debug_object},
};

static uint64_t aml_string_to_integer(aml_object_t* string, aml_object_t* dest)
{
    if (string->string.content == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (string->string.length == 0)
    {
        errno = EILSEQ;
        return ERR;
    }

    uint64_t value = 0;
    for (uint64_t i = 0; i < string->string.length && i < sizeof(uint64_t) * 2; i++)
    {
        char chr = string->string.content[i];
        if (chr >= '0' && chr <= '9')
        {
            value = value * 16 + (chr - '0');
        }
        else if (chr >= 'a' && chr <= 'f')
        {
            value = value * 16 + (chr - 'a' + 10);
        }
        else if (chr >= 'A' && chr <= 'F')
        {
            value = value * 16 + (chr - 'A' + 10);
        }
        else
        {
            break;
        }
    }

    if (aml_object_init_integer(dest, value) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_string_to_buffer(aml_object_t* string, aml_object_t* dest)
{
    if (aml_object_init_buffer_empty(dest, string->string.length + 1) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < string->string.length; i++)
    {
        dest->buffer.content[i] = (uint8_t)string->string.content[i];
    }
    dest->buffer.content[string->string.length] = 0;

    return 0;
}

static uint64_t aml_string_to_debug_object(aml_object_t* string, aml_object_t* dest)
{
    (void)string;
    (void)dest;

    // Debug objects are unimplemented.
    errno = ENOSYS;
    return ERR;
}

static aml_convert_entry_t stringConverters[AML_DATA_TYPE_AMOUNT] = {
    {AML_DATA_STRING, AML_DATA_INTEGER, aml_string_to_integer},
    {AML_DATA_STRING, AML_DATA_BUFFER, aml_string_to_buffer},
    {AML_DATA_STRING, AML_DATA_DEBUG_OBJECT, aml_string_to_debug_object},
};

uint64_t aml_convert_and_store(aml_object_t* src, aml_object_t* dest, aml_data_type_t allowedTypes)
{
    if (dest == NULL || src == NULL)
    {
        LOG_ERR("source or destination object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_DATA_UNINITALIZED)
    {
        LOG_ERR("source object is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    // BufferFields and FieldUnits are treated as either Buffers or Integers depending on their size.
    if (src->type == AML_DATA_FIELD_UNIT || src->type == AML_DATA_BUFFER_FIELD)
    {
        // Depending on the size temp becomes either a Buffer or an Integer.
        aml_object_t temp = AML_OBJECT_CREATE(AML_OBJECT_NONE);
        if (src->type == AML_DATA_FIELD_UNIT)
        {
            if (aml_field_unit_load(src, &temp) == ERR)
            {
                LOG_ERR("failed to load FieldUnit\n");
                return ERR;
            }
        }
        else
        {
            if (aml_buffer_field_load(src, &temp) == ERR)
            {
                LOG_ERR("failed to load BufferField\n");
                return ERR;
            }
        }

        if (temp.type & allowedTypes)
        {
            if (aml_copy_raw(&temp, dest) == ERR)
            {
                LOG_ERR("failed to clone loaded FieldUnit to destination\n");
                aml_object_deinit(&temp);
                return ERR;
            }

            aml_object_deinit(&temp);
            return 0;
        }

        uint64_t result = aml_convert_and_store(&temp, dest, allowedTypes);
        aml_object_deinit(&temp);
        return result;
    }

    if (src->type == dest->type && (src->type & allowedTypes))
    {
        if (aml_copy_raw(src, dest) == ERR)
        {
            LOG_ERR("failed to copy content of source object of type '%s' to destination object '%.*s'\n",
                aml_data_type_to_string(src->type), AML_NAME_LENGTH, dest->segment);
            return ERR;
        }
        return 0;
    }

    aml_convert_entry_t* converters;
    switch (src->type)
    {
    case AML_DATA_BUFFER:
        converters = bufferConverters;
        break;
    case AML_DATA_DEBUG_OBJECT:
        // Debug objects are unimplemented.
        errno = ENOSYS;
        return ERR;
    case AML_DATA_INTEGER:
        converters = integerConverters;
        break;
    case AML_DATA_INTEGER_CONSTANT:
        converters = integerConstantConverters;
        break;
    case AML_DATA_PACKAGE:
        converters = packageConverters;
        break;
    case AML_DATA_STRING:
        converters = stringConverters;
        break;
    default:
        LOG_ERR("no converters defined for source type '%s'\n", aml_data_type_to_string(src->type));
        errno = EINVAL;
        return ERR;
    }

    for (size_t i = 0; i < AML_DATA_TYPE_AMOUNT; i++)
    {
        aml_convert_entry_t* entry = &converters[i];

        if (entry->srcType == 0) // We have reached the end of the defined converters for this source type.
        {
            LOG_ERR("no valid converter found from type '%s' to any of the allowed types\n",
                aml_data_type_to_string(src->type));
            errno = EILSEQ;
            return ERR;
        }

        if (!(allowedTypes & entry->destType))
        {
            continue; // This destination type is not allowed, try the next converter.
        }

        if (entry->convertFunc == NULL)
        {
            LOG_ERR("no converter implemented from type '%s' to type '%s'\n", aml_data_type_to_string(src->type),
                aml_data_type_to_string(entry->destType));
            errno = ENOSYS;
            return ERR;
        }

        uint64_t result = entry->convertFunc(src, dest);
        if (result == ERR)
        {
            LOG_ERR("conversion from type '%s' to type '%s' failed\n", aml_data_type_to_string(src->type),
                aml_data_type_to_string(entry->destType));
            return ERR;
        }
        else if (result == AML_CONVERT_TRY_NEXT_CONVERTER)
        {
            continue;
        }

        return 0;
    }

    // Should never happen.
    errno = EILSEQ;
    return ERR;
}

uint64_t aml_convert_source(aml_object_t* source, aml_object_t* out, aml_data_type_t allowedTypes)
{
    if (source == NULL || out == NULL)
    {
        LOG_ERR("source or output object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (source->type == AML_DATA_UNINITALIZED)
    {
        LOG_ERR("source object '%.*s' is uninitialized\n", AML_NAME_LENGTH, source->segment);
        errno = EINVAL;
        return ERR;
    }

    // "If the operand is of the type expected by the operator, no conversion is necessary."
    if (source->type & allowedTypes)
    {
        if (aml_copy_raw(source, out) == ERR)
        {
            LOG_ERR("failed to copy content of source object '%.*s' of type '%s' to output object '%.*s'\n",
                AML_NAME_LENGTH, source->segment, aml_data_type_to_string(source->type), AML_NAME_LENGTH, out->segment);
            return ERR;
        }

        return 0;
    }

    // "If the operand type is incorrect, attempt to convert it to the proper type."
    if (aml_convert_and_store(source, out, allowedTypes) == ERR)
    {
        // "If conversion is impossible, abort the running control method and issue a fatal error."
        LOG_ERR("failed to convert source object '%.*s' of type '%s' to any of the allowed types\n", AML_NAME_LENGTH,
            source->segment, aml_data_type_to_string(source->type));
        return ERR;
    }

    // "For the Concatenate operator and logical operators (LEqual, LGreater, LGreaterEqual, LLess, LLessEqual, and
    // LNotEqual), the data type of the first operand dictates the required type of the second operand, and for Concate-
    // nate only, the type of the result object. (The second operator is implicitly converted, if necessary, to match
    // the type of the first operand.)"
    // We ignore this as it will be handled by the operator itself.

    return 0;
}

uint64_t aml_convert_result(aml_object_t* result, aml_object_t* target)
{
    if (result == NULL)
    {
        LOG_ERR("result object is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (result->type == AML_DATA_UNINITALIZED)
    {
        LOG_ERR("result object '%.*s' is uninitialized\n", AML_NAME_LENGTH, result->segment);
        errno = EINVAL;
        return ERR;
    }

    if (target == NULL)
    {
        return 0;
    }

    // "If the ASL operator is one of the explicit conversion operators (ToString, ToInteger, etc., and the CopyObject
    // operator), no conversion is performed. (In other words, the result object is stored directly to the target and
    // completely overwrites any existing object already stored at the target.)"
    // We ignore this as it will be handled by the operator itself.

    // "If the target is a method local or argument (LocalX or ArgX), no conversion is performed and the result is
    // stored directly to the target."
    if (target->flags & (AML_OBJECT_LOCAL | AML_OBJECT_ARG))
    {
        if (aml_copy_raw(result, target) == ERR)
        {
            LOG_ERR("failed to copy the content of result object '%.*s' to target local/arg object '%.*s'\n",
                AML_NAME_LENGTH, result->segment, AML_NAME_LENGTH, target->segment);
            return ERR;
        }
        return 0;
    }

    if (target->type == AML_DATA_UNINITALIZED)
    {
        LOG_ERR("target object '%.*s' is uninitialized\n", AML_NAME_LENGTH, target->segment);
        errno = EINVAL;
        return ERR;
    }

    // "If the target is a fixed type such as a named object or field object, an attempt is made to convert the source
    // to the existing target type before storing."
    if (aml_convert_and_store(result, target, target->type) == ERR)
    {
        // "If conversion is impossible, abort the running control method and issue a fatal error."
        LOG_ERR("failed to convert result object '%.*s' of type '%s' to target object '%.*s' of type '%s'\n",
            AML_NAME_LENGTH, result->segment, aml_data_type_to_string(result->type), AML_NAME_LENGTH, target->segment,
            aml_data_type_to_string(target->type));
        return ERR;
    }

    return 0;
}
