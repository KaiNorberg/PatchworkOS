#include "evaluate.h"
#include "store.h"

#include "acpi/aml/aml_to_string.h"
#include "lock_rule.h"
#include "access_type.h"
#include "log/log.h"

#include <errno.h>

uint64_t aml_evaluate(aml_node_t* node, aml_data_object_t* out, aml_term_arg_list_t* args)
{
    if (node == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_NODE_METHOD && args != NULL && args->count != 0)
    {
        LOG_ERR("attempted to pass %d arguments to non-method node '%.*s' of type '%s'\n",
            args == NULL ? 0 : args->count, AML_NAME_LENGTH, node->name, aml_node_type_to_string(node->type));
        errno = EILSEQ;
        return ERR;
    }

    bool mutexAcquired = false;
    if (aml_should_acquire_global_mutex(node))
    {
        mutex_acquire(&node->data.mutex.mutex);
        mutexAcquired = true;
    }

    uint64_t result = 0;
    switch (node->type)
    {
    case AML_NODE_NAME: // Section 19.6.90
    {
        memcpy(out, &node->data.name.object, sizeof(aml_data_object_t));
        result = 0;
    }
    break;
    case AML_NODE_INDEX_FIELD: // Section 19.6.64
    {
        aml_bit_size_t alignedBits = 0;
        aml_bit_size_t remainder = 0;
        aml_align_bits(node->data.indexField.bitOffset, node->data.indexField.flags.accessType, &alignedBits,
            &remainder);

        uint64_t byteOffset = alignedBits / 8;

        // "The value written to the IndexName register is defined to be a byte offset that is aligned on an AccessType
        // boundary." - Section 19.6.64
        // Honestly just good luck with the alignment stuff.
        aml_data_object_t index = AML_DATA_OBJECT_INTEGER(byteOffset);
        if (aml_store(node->data.indexField.indexNode, &index) == ERR)
        {
            result = ERR;
            break;
        }

        aml_data_object_t data;
        if (aml_evaluate(node->data.indexField.dataNode, &data, NULL) == ERR)
        {
            result = ERR;
            break;
        }

        if (data.type != AML_DATA_INTEGER)
        {
            LOG_ERR("IndexField DataNode '%.*s' did not evaluate to an integer\n", AML_NAME_LENGTH,
                node->data.indexField.dataNode->name);
            errno = EILSEQ;
            result = ERR;
            break;
        }

        uint64_t shiftedValue = data.integer >> remainder;
        uint64_t mask = (1ULL << node->data.indexField.bitSize) - 1;
        *out = AML_DATA_OBJECT_INTEGER(shiftedValue & mask);
        result = 0;
    }
    break;
    default:
    {
        LOG_ERR("unimplemented evaluation of node '%.*s' of type '%s'\n", AML_NAME_LENGTH, node->name,
            aml_node_type_to_string(node->type));
        errno = ENOSYS;
        result = ERR;
    }
    break;
    }

    if (mutexAcquired)
    {
        mutex_release(&node->data.mutex.mutex);
    }
    return result;
}
