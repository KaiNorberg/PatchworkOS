#include "store.h"

#include "access_type.h"
#include "acpi/aml/aml_to_string.h"
#include "lock_rule.h"
#include "log/log.h"

#include <errno.h>

uint64_t aml_store(aml_node_t* node, aml_data_object_t* object)
{
    if (node == NULL || object == NULL)
    {
        errno = EINVAL;
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
        memcpy(&node->data.name.object, object, sizeof(aml_data_object_t));
        result = 0;
    }
    break;
    case AML_NODE_FIELD: // Section 19.6.48
    {
        // Genuinely just good luck, field access is a... thing.

        if (object->type != AML_DATA_INTEGER)
        {
            LOG_ERR("attempted to store non-integer object to Field '%.*s'\n", AML_NAME_LENGTH, node->name);
            errno = EILSEQ;
            result = ERR;
            break;
        }
    }
    break;
    default:
    {
        LOG_ERR("unimplemented store to node '%.*s' of type '%s'\n", AML_NAME_LENGTH, node->name,
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
