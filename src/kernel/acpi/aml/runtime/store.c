#include "store.h"

#include "acpi/aml/aml_to_string.h"
#include "lock_rule.h"
#include "log/log.h"
#include "opregion.h"

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
        mutex_acquire_recursive(aml_global_mutex_get());
        mutexAcquired = true;
    }

    uint64_t result = 0;
    switch (node->type)
    {
    case AML_NODE_NAME: // Section 19.6.90
    {
        memcpy(&node->name.object, object, sizeof(aml_data_object_t));
        result = 0;
    }
    break;
    case AML_NODE_FIELD:
    {
        result = aml_field_write(node, object);
    }
    break;
    case AML_NODE_INDEX_FIELD:
    {
        result = aml_index_field_write(node, object);
    }
    break;
    default:
    {
        LOG_ERR("unimplemented store to node '%.*s' of type '%s'\n", AML_NAME_LENGTH, node->segment,
            aml_node_type_to_string(node->type));
        errno = ENOSYS;
        result = ERR;
    }
    break;
    }

    if (mutexAcquired)
    {
        mutex_release(aml_global_mutex_get());
    }
    return result;
}
