#include "aml.h"

#include "acpi/acpi.h"
#include "aml_debug.h"
#include "aml_state.h"
#include "aml_to_string.h"
#include "encoding/term.h"
#include "mem/heap.h"

#include "log/log.h"

#include <errno.h>
#include <stddef.h>

static mutex_t globalMutex;

static aml_node_t* root = NULL;

uint64_t aml_init(void)
{
    mutex_init(&globalMutex);

    root = aml_node_add(NULL, AML_ROOT_NAME, AML_NODE_PREDEFINED);
    if (root == NULL)
    {
        return ERR;
    }

    if (aml_node_add(root, "_SB_", AML_NODE_PREDEFINED) == NULL ||
        aml_node_add(root, "_SI_", AML_NODE_PREDEFINED) == NULL ||
        aml_node_add(root, "_GPE", AML_NODE_PREDEFINED) == NULL ||
        aml_node_add(root, "_PR_", AML_NODE_PREDEFINED) == NULL ||
        aml_node_add(root, "_TZ_", AML_NODE_PREDEFINED) == NULL)
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
    // as thats the `sdt_header_t`. So the entire code is a termlist.

    aml_state_t state;
    aml_state_init(&state, data, size);

    uint64_t result = aml_term_list_read(&state, aml_root_get(), size);

    aml_state_deinit(&state);
    return result;
}

aml_node_t* aml_node_add(aml_node_t* parent, const char* name, aml_node_type_t type)
{
    if (name == NULL || type < AML_NODE_NONE || type >= AML_NODE_MAX)
    {
        errno = EINVAL;
        return NULL;
    }

    MUTEX_SCOPE(&globalMutex);

    aml_node_t* node = heap_alloc(sizeof(aml_node_t), HEAP_NONE);
    if (node == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    list_entry_init(&node->entry);
    node->type = type;
    list_init(&node->children);
    memcpy(node->segment, name, AML_NAME_LENGTH);
    node->segment[AML_NAME_LENGTH] = '\0';

    if (parent != NULL)
    {
        if (parent->parent == NULL)
        {
            assert(root == parent);
        }

        if (sysfs_dir_init(&node->dir, &parent->dir, node->segment, NULL, NULL) == ERR)
        {
            LOG_ERR("failed to create sysfs directory for aml node '%.*s'\n", AML_NAME_LENGTH, name);
            heap_free(node);
            return NULL;
        }

        node->parent = parent;
        list_push(&parent->children, &node->entry);
    }
    else
    {
        assert(root == NULL);
        assert(strcmp(node->segment, AML_ROOT_NAME) == 0);

        if (sysfs_dir_init(&node->dir, acpi_get_sysfs_root(), "namespace", NULL, NULL) == ERR)
        {
            LOG_ERR("failed to create sysfs directory for aml node '%.*s'\n", AML_NAME_LENGTH, name);
            heap_free(node);
            return NULL;
        }

        node->parent = NULL;
    }

    return node;
}

aml_node_t* aml_node_add_at_name_string(aml_name_string_t* string, aml_node_t* start, aml_node_type_t type)
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
            if (memcmp(child->segment, segment->name, AML_NAME_LENGTH) == 0)
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            LOG_ERR("unable to find aml node '%.*s' under node '%.*s'\n", AML_NAME_LENGTH, segment->name,
                AML_NAME_LENGTH, parentNode->name);
            errno = ENOENT;
            return NULL;
        }
        parentNode = child;
    }

    aml_node_t* newNode =
        aml_node_add(parentNode, string->namePath.segments[string->namePath.segmentCount - 1].name, type);
    if (newNode == NULL)
    {
        return NULL;
    }

    return newNode;
}

aml_node_t* aml_node_find_child(aml_node_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    aml_node_t* child = NULL;
    LIST_FOR_EACH(child, &parent->children, entry)
    {
        if (memcmp(child->segment, name, AML_NAME_LENGTH) == 0)
        {
            return child;
        }
    }

    errno = ENOENT;
    return NULL;
}

aml_node_t* aml_node_find(const aml_name_string_t* nameString, aml_node_t* start)
{
    aml_node_t* current = start;
    if (current == NULL || nameString->rootChar.present)
    {
        current = aml_root_get();
    }

    for (uint64_t i = 0; i < nameString->prefixPath.depth; i++)
    {
        current = current->parent;
        if (current == NULL)
        {
            errno = ENOENT;
            return NULL;
        }
    }

    for (uint64_t i = 0; i < nameString->namePath.segmentCount; i++)
    {
        const aml_name_seg_t* segment = &nameString->namePath.segments[i];
        current = aml_node_find_child(current, segment->name);
        if (current == NULL)
        {
            return NULL;
        }
    }

    return current;
}

aml_node_t* aml_node_find_by_path(const char* path, aml_node_t* start)
{
    if (path == NULL || path[0] == '\0')
    {
        errno = EINVAL;
        return NULL;
    }

    const char* ptr = path;
    switch (*ptr)
    {
    case AML_ROOT_CHAR:
        start = aml_root_get();
        ptr++;
        break;
    case AML_PARENT_PREFIX_CHAR:
        if (start == NULL)
        {
            errno = EINVAL;
            return NULL;
        }
        while (*ptr == AML_PARENT_PREFIX_CHAR)
        {
            start = start->parent;
            if (start == NULL)
            {
                errno = ENOENT;
                return NULL;
            }
            ptr++;
        }
        break;
    default:
        if (start == NULL)
        {
            start = aml_root_get();
        }
        break;
    }

    aml_node_t* node = start;
    while (*ptr != '\0')
    {
        const char* segmentStart = ptr;
        while (*ptr != '.' && *ptr != '\0')
        {
            ptr++;
        }
        size_t segmentLength = ptr - segmentStart;
        if (segmentLength != AML_NAME_LENGTH)
        {
            errno = EILSEQ;
            return NULL;
        }

        char segment[AML_NAME_LENGTH + 1];
        memcpy(segment, segmentStart, AML_NAME_LENGTH);
        segment[AML_NAME_LENGTH] = '\0';

        aml_node_t* child = aml_node_find_child(node, segment);
        if (child == NULL)
        {
            return NULL;
        }
        node = child;

        if (*ptr == '.')
        {
            ptr++;
        }
    }

    return node;
}

mutex_t* aml_global_mutex_get(void)
{
    return &globalMutex;
}

aml_node_t* aml_root_get(void)
{
    MUTEX_SCOPE(&globalMutex);

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
