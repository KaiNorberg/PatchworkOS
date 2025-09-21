#include "aml.h"

#include "aml_state.h"
#include "aml_to_string.h"
#include "encoding/term.h"
#include "log/log.h"
#include "runtime/lock_rule.h"
#include "runtime/opregion.h"

#include "log/log.h"

#include <errno.h>
#include <sys/math.h>

static mutex_t globalMutex;

static aml_node_t* root = NULL;

uint64_t aml_init(void)
{
    mutex_init(&globalMutex);

    root = aml_node_new(NULL, AML_ROOT_NAME, AML_NODE_PREDEFINED);
    if (root == NULL)
    {
        return ERR;
    }

    // Normal predefined root nodes, see section 5.3.1 of the ACPI specification.
    if (aml_node_new(root, "_GPE", AML_NODE_PREDEFINED) == NULL ||
        aml_node_new(root, "_PR", AML_NODE_PREDEFINED) == NULL ||
        aml_node_new(root, "_SB", AML_NODE_PREDEFINED) == NULL ||
        aml_node_new(root, "_SI", AML_NODE_PREDEFINED) == NULL ||
        aml_node_new(root, "_TZ", AML_NODE_PREDEFINED) == NULL)
    {
        return ERR;
    }

    // OS specific predefined nodes, see section 5.7 of the ACPI specification.
    // We define their behaviour as edge cases in the AML parser.
    if (aml_node_new(root, "_GL", AML_NODE_PREDEFINED_GL) == NULL ||
        aml_node_new(root, "_OS", AML_NODE_PREDEFINED_OS) == NULL ||
        aml_node_new(root, "_OSI", AML_NODE_PREDEFINED_OSI) == NULL ||
        aml_node_new(root, "_REV", AML_NODE_PREDEFINED_REV) == NULL)
    {
        return ERR;
    }

    return 0;
}

uint64_t aml_parse(const void* data, uint64_t size)
{
    if (data == NULL || size == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    // In section 20.2.1, we see the definition AMLCode := DefBlockHeader TermList.
    // The DefBlockHeader is already read as thats the `sdt_header_t`.
    // So the entire code is a termlist.

    aml_state_t state;
    if (aml_state_init(&state, data, size) == ERR)
    {
        return ERR;
    }

    uint64_t result = aml_term_list_read(&state, aml_root_get(), size);

    aml_state_deinit(&state);
    return result;
}

uint64_t aml_evaluate(aml_node_t* node, aml_data_object_t* out, aml_term_arg_list_t* args)
{
    if (node == NULL || out == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    MUTEX_SCOPE(&node->lock);

    uint64_t expectedArgCount = aml_node_get_expected_arg_count(node);
    if (expectedArgCount == ERR)
    {
        return ERR;
    }

    if (args != NULL && args->count != 0)
    {
        if (args->count != expectedArgCount)
        {
            LOG_ERR("node '%.*s' of type '%s' expects %u arguments, but %u were provided\n", AML_NAME_LENGTH,
                node->segment, aml_node_type_to_string(node->type), expectedArgCount, args->count);
            errno = EINVAL;
            return ERR;
        }
    }
    else if (expectedArgCount != 0)
    {
        LOG_ERR("node '%.*s' of type '%s' expects %u arguments, but none were provided\n", AML_NAME_LENGTH,
            node->segment, aml_node_type_to_string(node->type), expectedArgCount);
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
    case AML_NODE_PREDEFINED_OSI:
    {
        // TODO: Implement _OSI properly, for now we just always return true.
        return aml_data_object_init_integer(out, 1, 64);
    }
    case AML_NODE_NAME: // Section 19.6.90
    {
        memcpy(out, &node->name.object, sizeof(aml_data_object_t));
        result = 0;
    }
    break;
    case AML_NODE_FIELD:
    {
        result = aml_field_read(node, out);
    }
    break;
    case AML_NODE_INDEX_FIELD:
    {
        result = aml_index_field_read(node, out);
    }
    break;
    case AML_NODE_BANK_FIELD:
    {
        result = aml_bank_field_read(node, out);
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
        mutex_release(aml_global_mutex_get());
    }
    return result;
}

uint64_t aml_store(aml_node_t* node, aml_data_object_t* object)
{
    if (node == NULL || object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    MUTEX_SCOPE(&node->lock);

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
    case AML_NODE_BANK_FIELD:
    {
        result = aml_bank_field_write(node, object);
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

mutex_t* aml_global_mutex_get(void)
{
    return &globalMutex;
}

aml_node_t* aml_root_get(void)
{
    if (root == NULL)
    {
        errno = ENOSYS;
        return NULL;
    }

    return root;
}

void aml_print_tree(aml_node_t* node, uint32_t depth, bool isLast)
{
    for (uint32_t i = 0; i < depth; i++)
    {
        if (i == depth - 1)
        {
            LOG_INFO("%s", isLast ? "└── " : "├── ");
        }
        else
        {
            LOG_INFO("│   ");
        }
    }

    LOG_INFO("%.*s [%s", AML_NAME_LENGTH, node->segment, aml_node_type_to_string(node->type));
    switch (node->type)
    {
    case AML_NODE_OPREGION:
        LOG_INFO(": space=%s, offset=0x%x, length=0x%x", aml_region_space_to_string(node->opregion.space),
            node->opregion.offset, node->opregion.length);
        break;
    case AML_NODE_FIELD:
        LOG_INFO(": accessType=%s, lockRule=%s, updateRule=%s, offset=0x%x, size=%llu",
            aml_access_type_to_string(node->field.flags.accessType),
            aml_lock_rule_to_string(node->field.flags.lockRule),
            aml_update_rule_to_string(node->field.flags.updateRule), node->field.bitOffset, node->field.bitSize);
        break;
    case AML_NODE_METHOD:
        LOG_INFO(": argCount=%u, serialized=%s, syncLevel=%d, start=0x%x, end=0x%x", node->method.flags.argCount,
            node->method.flags.isSerialized ? "true" : "false", node->method.flags.syncLevel, node->method.start,
            node->method.end);
        break;
    case AML_NODE_NAME:
        LOG_INFO(": object=%s, dataType=%s", aml_data_object_to_string(&node->name.object),
            aml_data_type_to_string(node->name.object.type));
        break;
    case AML_NODE_MUTEX:
        LOG_INFO(": syncLevel=%d", node->mutex.syncLevel);
        break;
    default:
        break;
    }
    LOG_INFO("]\n");

    aml_node_t* child;
    LIST_FOR_EACH(child, &node->children, entry)
    {
        aml_print_tree(child, depth + 1, list_last(&node->children) == &child->entry);
    }
}
