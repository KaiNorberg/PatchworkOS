#include "opregion.h"

#include "log/log.h"

#include <errno.h>

uint64_t aml_field_read(aml_node_t* field, aml_data_object_t* out)
{
    if (field == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (field->type != AML_NODE_FIELD && field->type != AML_NODE_INDEX_FIELD && field->type != AML_NODE_BANK_FIELD)
    {
        errno = EINVAL;
        return ERR;
    }

    if (field->type == AML_NODE_BANK_FIELD)
    {
        LOG_ERR("BankField read not implemented\n");
        errno = ENOSYS;
        return ERR;
    }

    aml_bit_size_t bitSize = field->type == AML_NODE_FIELD ? field->data.field.bitSize : field->data.indexField.bitSize;
    uint64_t byteSize = (bitSize + 7) / 8;

    if (byteSize > sizeof(aml_qword_data_t))
    {
        if (aml_data_object_init_buffer(out, byteSize) == ERR)
        {
            return ERR;
        }
    }
}
