#include "aml_convert.h"

#include "aml.h"
#include "aml_to_string.h"
#include "log/log.h"
#include "runtime/lock_rule.h"

#include <errno.h>

uint64_t aml_convert_to_actual_data(aml_node_t* src, aml_node_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (src == dest)
    {
        return 0;
    }

    aml_data_type_info_t* srcInfo = aml_data_type_get_info(src->type);
    if (srcInfo->flags & AML_DATA_FLAG_NONE)
    {
        errno = EINVAL;
        return ERR;
    }

    if (srcInfo->flags & AML_DATA_FLAG_IS_ACTUAL_DATA)
    {
        return aml_node_clone(src, dest);
    }

    if (!(srcInfo->flags & AML_DATA_FLAG_DATA_OBJECT))
    {
        errno = EILSEQ;
        return ERR;
    }

    switch (src->type)
    {
    case AML_DATA_BUFFER_FIELD:
    {
        LOG_ERR("unimplemented conversion of buffer field to actual data\n");
        errno = ENOSYS;
        return ERR;
    }
    break;
    case AML_DATA_FIELD_UNIT:
    {
        LOG_ERR("unimplemented conversion of field unit to actual data\n");
        errno = ENOSYS;
        return ERR;
    }
    break;
    default:
        errno = ENOSYS;
        return ERR;
        break;
    }

    return 0;
}

uint64_t aml_convert_and_store(aml_node_t* src, aml_node_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (src == dest)
    {
        return 0;
    }

    switch (src->type)
    {
    case AML_DATA_UNINITALIZED:
        errno = EINVAL;
        return ERR;
    default:
        LOG_ERR("unimplemented conversion from '%s' to '%s'\n", aml_data_type_to_string(src->type),
            aml_data_type_to_string(dest->type));
        errno = ENOSYS;
        return ERR;
    }

    return 0;
}

uint64_t aml_convert_to_integer(aml_node_t* src, aml_node_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (src == dest)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (src->type)
    {
    case AML_DATA_INTEGER:
        return aml_node_clone(src, dest);
    case AML_DATA_INTEGER_CONSTANT:
        return aml_node_init_integer(dest, src->integerConstant.value);
    case AML_DATA_BUFFER:
    {
        if (src->buffer.length == 0)
        {
            errno = EINVAL;
            return ERR;
        }

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
    case AML_DATA_STRING:
    {
        if (src->string.content == NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        uint64_t value = 0;
        uint64_t len = strlen(src->string.content);
        for (uint64_t i = 0; i < len && i < sizeof(uint64_t) * 2; i++)
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
    default:
        LOG_ERR("invalid conversion from '%s' to Integer\n", aml_data_type_to_string(src->type));
        errno = ENOSYS;
        return ERR;
    }
}
