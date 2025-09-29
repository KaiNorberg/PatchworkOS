#include "evaluate.h"

#include "acpi/aml/aml_node.h"
#include "acpi/aml/aml_to_string.h"
#include "buffer_field.h"
#include "log/log.h"
#include "method.h"
#include "opregion.h"

#include <errno.h>

#define AML_CONVERT_TRY_NEXT_CONVERTER 1

typedef uint64_t (*aml_convert_func_t)(aml_node_t* src, aml_node_t* dest);

typedef struct
{
    aml_data_type_t srcType;
    aml_data_type_t destType;
    aml_convert_func_t convertFunc;
} aml_convert_entry_t;

// We create the arrays of converters here. The order of the list defines the priority of the converters. First ==
// Highest Priority. Last == Lowest Priority. See section 19.3.5.7 table 19.6 for the conversion priority order.

static uint64_t aml_buffer_to_integer(aml_node_t* src, aml_node_t* dest)
{
    uint64_t value = 0;
    for (uint64_t i = 0; i < src->buffer.length && i < sizeof(uint64_t); i++)
    {
        value |= ((uint64_t)src->buffer.content[i]) << (i * 8);
    }

    if (aml_node_init_integer(dest, value) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_buffer_to_string(aml_node_t* src, aml_node_t* dest)
{
    // Each byte in the buffer becomes two hex chars with a space in between.
    uint64_t length = src->buffer.length * 2 + (src->buffer.length > 0 ? src->buffer.length - 1 : 0);

    // Will add the null terminator on its own.
    if (aml_node_init_string_empty(dest, length) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < src->buffer.length; i++)
    {
        // Honestly just did it this way becouse it looks cool.
        uint8_t byte = src->buffer.content[i];
        dest->string.content[i * 3 + 0] = "0123456789ABCDEF"[byte >> 4];
        dest->string.content[i * 3 + 1] = "0123456789ABCDEF"[byte & 0x0F];
        if (i < src->buffer.length - 1)
        {
            dest->string.content[i * 3 + 2] = ' ';
        }
    }

    return 0;
}

static uint64_t aml_buffer_to_debug_object(aml_node_t* src, aml_node_t* dest)
{
    (void)src;
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

static uint64_t aml_integer_to_buffer(aml_node_t* src, aml_node_t* dest)
{
    if (dest->type != AML_DATA_BUFFER)
    {
        if (aml_node_init_buffer_empty(dest, sizeof(uint64_t)) == ERR)
        {
            return ERR;
        }
    }

    uint64_t i = 0;
    for (; i < sizeof(uint64_t); i++)
    {
        dest->buffer.content[i] = (src->integer.value >> (i * 8)) & 0xFF;
    }
    for (; i < dest->buffer.length; i++)
    {
        dest->buffer.content[i] = 0;
    }

    return 0;
}

static uint64_t aml_integer_to_buffer_field(aml_node_t* src, aml_node_t* dest)
{
    if (dest->type != AML_DATA_BUFFER_FIELD)
    {
        return AML_CONVERT_TRY_NEXT_CONVERTER;
    }

    if (aml_buffer_field_store(src, dest) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_integer_to_string(aml_node_t* src, aml_node_t* dest)
{
    uint64_t stringLength = sizeof(uint64_t) * 2; // Each byte becomes two hex chars.

    if (aml_node_init_string_empty(dest, stringLength) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < sizeof(uint64_t); i++)
    {
        uint8_t byte = (src->integer.value >> (i * 8)) & 0xFF;
        dest->string.content[i * 2 + 0] = "0123456789ABCDEF"[byte >> 4];
        dest->string.content[i * 2 + 1] = "0123456789ABCDEF"[byte & 0x0F];
    }

    return 0;
}

static uint64_t aml_integer_to_debug_object(aml_node_t* src, aml_node_t* dest)
{
    (void)src;
    (void)dest;

    // Debug objects are unimplemented.
    errno = ENOSYS;
    return ERR;
}

static aml_convert_entry_t integerConverters[AML_DATA_TYPE_AMOUNT] = {
    {AML_DATA_INTEGER, AML_DATA_BUFFER, aml_integer_to_buffer},
    {AML_DATA_INTEGER, AML_DATA_BUFFER_FIELD, aml_integer_to_buffer_field},
    {AML_DATA_INTEGER, AML_DATA_FIELD_UNIT, aml_integer_to_buffer_field},
    {AML_DATA_INTEGER, AML_DATA_STRING, aml_integer_to_string},
    {AML_DATA_INTEGER, AML_DATA_DEBUG_OBJECT, aml_integer_to_debug_object},
};

static uint64_t aml_integer_constant_to_integer(aml_node_t* src, aml_node_t* dest)
{
    if (aml_node_init_integer(dest, src->integerConstant.value) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_integer_constant_to_debug_object(aml_node_t* src, aml_node_t* dest)
{
    (void)src;
    (void)dest;

    // Debug objects are unimplemented.
    errno = ENOSYS;
    return ERR;
}

static aml_convert_entry_t integerConstantConverters[AML_DATA_TYPE_AMOUNT] = {
    {AML_DATA_INTEGER_CONSTANT, AML_DATA_INTEGER, aml_integer_constant_to_integer},
    {AML_DATA_INTEGER_CONSTANT, AML_DATA_DEBUG_OBJECT, aml_integer_constant_to_debug_object},
};

static uint64_t aml_package_to_debug_object(aml_node_t* src, aml_node_t* dest)
{
    (void)src;
    (void)dest;

    // Debug objects are unimplemented.
    errno = ENOSYS;
    return ERR;
}

static aml_convert_entry_t packageConverters[AML_DATA_TYPE_AMOUNT] = {
    {AML_DATA_PACKAGE, AML_DATA_DEBUG_OBJECT, aml_package_to_debug_object},
};

static uint64_t aml_string_to_integer(aml_node_t* src, aml_node_t* dest)
{
    if (src->string.content == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    uint64_t value = 0;
    for (uint64_t i = 0; i < src->string.length && i < sizeof(uint64_t) * 2; i++)
    {
        char chr = src->string.content[i];
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

    if (aml_node_init_integer(dest, value) == ERR)
    {
        return ERR;
    }

    return 0;
}

static uint64_t aml_string_to_buffer(aml_node_t* src, aml_node_t* dest)
{
    if (aml_node_init_buffer_empty(dest, src->string.length + 1) == ERR)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < src->string.length; i++)
    {
        dest->buffer.content[i] = (uint8_t)src->string.content[i];
    }
    dest->buffer.content[src->string.length] = 0;

    return 0;
}

static uint64_t aml_string_to_debug_object(aml_node_t* src, aml_node_t* dest)
{
    (void)src;
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

uint64_t aml_evaluate(aml_node_t* src, aml_node_t* dest, aml_data_type_t allowedTypes)
{
    if (dest == NULL || src == NULL)
    {
        LOG_ERR("source or destination node is NULL\n");
        errno = EINVAL;
        return ERR;
    }

    if (src->type == AML_DATA_UNINITALIZED)
    {
        LOG_ERR("source node is uninitialized\n");
        errno = EINVAL;
        return ERR;
    }

    if ((src->type == dest->type || dest->type == AML_DATA_UNINITALIZED) && (allowedTypes & src->type))
    {
        if (aml_node_clone(src, dest) == ERR)
        {
            LOG_ERR("failed to clone node\n");
            return ERR;
        }
        return 0;
    }

    // BufferFields and FieldUnits are treated as either Buffers or Integers depending on their size.
    if (src->type == AML_DATA_BUFFER_FIELD)
    {
        // Depending on the size temp becomes either a Buffer or an Integer.
        aml_node_t temp = AML_NODE_CREATE;
        if (aml_buffer_field_load(src, &temp) == ERR)
        {
            LOG_ERR("failed to load BufferField\n");
            return ERR;
        }

        uint64_t result = aml_evaluate(&temp, dest, allowedTypes);
        aml_node_deinit(&temp);
        return result;
    }
    else if (src->type == AML_DATA_FIELD_UNIT)
    {
        // Depending on the size temp becomes either a Buffer or an Integer.
        aml_node_t temp = AML_NODE_CREATE;
        if (aml_field_unit_load(src, &temp) == ERR)
        {
            LOG_ERR("failed to load FieldUnit\n");
            return ERR;
        }

        uint64_t result = aml_evaluate(&temp, dest, allowedTypes);
        aml_node_deinit(&temp);
        return result;
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

        if (entry->srcType != src->type) // We have reached the end of the defined converters for this source type.
        {
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

    // Should never reach here.
    errno = EILSEQ;
    return ERR;
}
