#include "aml.h"

#include "aml_state.h"
#include "aml_to_string.h"
#include "encoding/term.h"
#include "mem/heap.h"

#include "log/log.h"

#include <errno.h>
#include <stddef.h>

static mutex_t mutex;

static aml_node_t* root = NULL;

uint64_t aml_init(void)
{
    mutex_init(&mutex);

    root = aml_add_node(NULL, "\\___", AML_NODE_PREDEFINED);
    if (root == NULL)
    {
        return ERR;
    }

    if (aml_add_node(root, "_SB_", AML_NODE_PREDEFINED) == NULL ||
        aml_add_node(root, "_SI_", AML_NODE_PREDEFINED) == NULL ||
        aml_add_node(root, "_GPE", AML_NODE_PREDEFINED) == NULL ||
        aml_add_node(root, "_PR_", AML_NODE_PREDEFINED) == NULL ||
        aml_add_node(root, "_TZ_", AML_NODE_PREDEFINED) == NULL)
    {
        return ERR;
    }

    // TODO: Add os predefined nodes like _OSI, _REV, etc.

    return 0;
}

uint64_t aml_parse(const void* data, uint64_t size)
{
    if (data == NULL || size == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    // In section 20.2.1, we see the definition AMLCode := DefBlockHeader TermList. The DefBlockHeader is already read
    // as thats the `acpi_header_t`. So the entire code is a termlist.

    aml_state_t state;
    aml_state_init(&state, data, size);

    // When aml first starts its not in any node, so we pass NULL as the node.
    uint64_t result = aml_termlist_read(&state, NULL, size);

    aml_state_deinit(&state);
    return result;
}

aml_node_t* aml_add_node(aml_node_t* parent, const char* name, aml_node_type_t type)
{
    if (name == NULL || type < AML_NODE_NONE ||
        type >= AML_NODE_MAX)
    {
        errno = EINVAL;
        return NULL;
    }

    MUTEX_SCOPE(&mutex);

    aml_node_t* node = heap_alloc(sizeof(aml_node_t), HEAP_NONE);
    if (node == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    list_entry_init(&node->entry);
    node->type = type;
    list_init(&node->children);
    memcpy(node->name, name, AML_NAME_LENGTH);

    if (parent != NULL)
    {
        node->parent = parent;
        list_push(&parent->children, &node->entry);
    }
    else
    {
        node->parent = NULL;
    }

    return node;
}

aml_node_t* aml_add_node_at_name_string(aml_name_string_t* string, aml_node_t* start, aml_node_type_t type)
{
    if (string->namePath.segmentCount == 0)
    {
        errno = EILSEQ;
        return NULL;
    }

    if (start == NULL || string->rootChar.present)
    {
        start = aml_root_get();
    }

    for (uint64_t i = 0; i < string->prefixPath.depth; i++)
    {
        start = start->parent;
        if (start == NULL)
        {
            errno = ENOENT;
            return NULL;
        }
    }

    aml_node_t* parentNode = start;
    for (uint64_t i = 1; i < string->namePath.segmentCount; i++)
    {
        bool found = false;
        const aml_name_seg_t* segment = &string->namePath.segments[i - 1];
        aml_node_t* child = NULL;
        LIST_FOR_EACH(child, &parentNode->children, entry)
        {
            if (memcmp(child->name, segment->name, AML_NAME_LENGTH) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG_ERR("unable to find aml node '%.*s' under node '%.*s'\n", AML_NAME_LENGTH,
                segment->name, AML_NAME_LENGTH, parentNode->name);
            errno = ENOENT;
            return NULL;
        }
        parentNode = child;
    }

    aml_node_t* newNode =
        aml_add_node(parentNode, string->namePath.segments[string->namePath.segmentCount - 1].name, type);
    if (newNode == NULL)
    {
        return NULL;
    }

    return newNode;
}

aml_node_t* aml_find_node(const char* path, aml_node_t* start)
{
    MUTEX_SCOPE(&mutex);

    // TODO: Implement aml_find_node
    errno = ENOSYS;
    return NULL;
}

aml_node_t* aml_find_node_name_string(const aml_name_string_t* nameString, aml_node_t* start)
{
    if (start == NULL || nameString->rootChar.present)
    {
        start = aml_root_get();
    }

    for (uint64_t i = 0; i < nameString->prefixPath.depth; i++)
    {
        start = start->parent;
        if (start == NULL)
        {
            errno = ENOENT;
            return NULL;
        }
    }

    aml_node_t* found = start;
    for (uint64_t i = 0; i < nameString->namePath.segmentCount; i++)
    {
        const aml_name_seg_t* segment = &nameString->namePath.segments[i];
        bool foundChild = false;
        aml_node_t* child = NULL;
        LIST_FOR_EACH(child, &found->children, entry)
        {
            if (memcmp(child->name, segment->name, AML_NAME_LENGTH) == 0)
            {
                foundChild = true;
                break;
            }
        }

        if (child == NULL || !foundChild)
        {
            errno = ENOENT;
            return NULL;
        }
        found = child;
    }

    return found;
}

aml_node_t* aml_root_get(void)
{
    MUTEX_SCOPE(&mutex);

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

    LOG_INFO("%.*s [%s", AML_NAME_LENGTH, node->name, aml_node_type_to_string(node->type));
    switch (node->type)
    {
    case AML_NODE_OPREGION:
        LOG_INFO(": space=%s, offset=0x%x, length=0x%x", aml_region_space_to_string(node->data.opregion.space),
            node->data.opregion.offset, node->data.opregion.length);
        break;
    case AML_NODE_FIELD:
        LOG_INFO(": accessType=%s, lockRule=%s, updateRule=%s, offset=0x%x, size=%llu",
            aml_access_type_to_string(node->data.field.flags.accessType),
            aml_lock_rule_to_string(node->data.field.flags.lockRule),
            aml_update_rule_to_string(node->data.field.flags.updateRule), node->data.field.offset,
            node->data.field.size);
        break;
    case AML_NODE_METHOD:
        LOG_INFO(": argCount=%u, serialized=%s, syncLevel=%d, start=0x%x, end=0x%x", node->data.method.flags.argCount,
            node->data.method.flags.isSerialized ? "true" : "false", node->data.method.flags.syncLevel,
            node->data.method.start, node->data.method.end);
        break;
    case AML_NODE_NAME:
        LOG_INFO(": object=%s", aml_data_object_to_string(&node->data.name.object));
        break;
    case AML_NODE_MUTEX:
        LOG_INFO(": syncLevel=%d", node->data.mutex.syncLevel);
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
