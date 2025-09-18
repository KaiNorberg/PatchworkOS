#include "evaluate.h"
#include "store.h"

#include "access_type.h"
#include "acpi/aml/aml_to_string.h"
#include "lock_rule.h"
#include "log/log.h"
#include "opregion.h"

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
            args == NULL ? 0 : args->count, AML_NAME_LENGTH, node->segment, aml_node_type_to_string(node->type));
        errno = EILSEQ;
        return ERR;
    }

    bool mutexAcquired = false;
    if (aml_should_acquire_global_mutex(node))
    {
        mutex_acquire(&node->mutex.mutex);
        mutexAcquired = true;
    }

    uint64_t result = 0;
    switch (node->type)
    {
    case AML_NODE_NAME: // Section 19.6.90
    {
        memcpy(out, &node->name.object, sizeof(aml_data_object_t));
        result = 0;
    }
    break;
    case AML_NODE_INDEX_FIELD:
    case AML_NODE_BANK_FIELD:
    case AML_NODE_FIELD:
    {
        result = aml_field_read(node, out);
    }
    break;
    default:
    {
        LOG_ERR("unimplemented evaluation of node '%.*s' of type '%s'\n", AML_NAME_LENGTH, node->segment,
            aml_node_type_to_string(node->type));
        errno = ENOSYS;
        result = ERR;
    }
    break;
    }

    if (mutexAcquired)
    {
        mutex_release(&node->mutex.mutex);
    }
    return result;
}
