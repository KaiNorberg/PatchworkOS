#include "aml.h"

#include "acpi/acpi.h"
#include "aml_state.h"
#include "aml_to_string.h"
#include "aml_value.h"
#include "encoding/term.h"
#include "log/log.h"
#include "mem/heap.h"
#include "runtime/lock_rule.h"
#include "runtime/opregion.h"

#include "log/log.h"

#include <errno.h>
#include <stddef.h>
#include <sys/math.h>

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

    // Normal predefined root nodes, see section 5.3.1 of the ACPI specification.
    if (aml_node_add(root, "_GPE", AML_NODE_PREDEFINED) == NULL ||
        aml_node_add(root, "_PR", AML_NODE_PREDEFINED) == NULL ||
        aml_node_add(root, "_SB", AML_NODE_PREDEFINED) == NULL ||
        aml_node_add(root, "_SI", AML_NODE_PREDEFINED) == NULL ||
        aml_node_add(root, "_TZ", AML_NODE_PREDEFINED) == NULL)
    {
        return ERR;
    }

    // OS specific predefined nodes, see section 5.7 of the ACPI specification.
    // We define their behaviour as edge cases in the AML parser.
    if (aml_node_add(root, "_GL", AML_NODE_PREDEFINED_GL) == NULL ||
        aml_node_add(root, "_OS", AML_NODE_PREDEFINED_OS) == NULL ||
        aml_node_add(root, "_OSI", AML_NODE_PREDEFINED_OSI) == NULL ||
        aml_node_add(root, "_REV", AML_NODE_PREDEFINED_REV) == NULL)
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
    aml_state_init(&state, data, size, NULL);

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

    uint64_t expectedArgCount = aml_node_get_expected_arg_count(node);
    if (expectedArgCount == ERR)
    {
        return ERR;
    }

    if (args != NULL && args->count != 0)
    {
        if (args->count != expectedArgCount)
        {
            LOG_ERR("node '%.*s' of type '%s' expects %u arguments, but %u were provided\n",
                AML_NAME_LENGTH, node->segment, aml_node_type_to_string(node->type), expectedArgCount,
                args->count);
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

uint64_t aml_node_get_expected_arg_count(aml_node_t* node)
{
    if (node == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (node->type)
    {
    case AML_NODE_PREDEFINED_OSI:
        return 1;
    case AML_NODE_METHOD:
        return node->method.flags.argCount;
    default:
        return 0;
    }
}

bool aml_is_name_equal(const char* s1, const char* s2)
{
    const char* end1 = s1 + strnlen_s(s1, AML_NAME_LENGTH);
    while (end1 > s1 && *(end1 - 1) == '_')
    {
        end1--;
    }

    const char* end2 = s2 + strnlen_s(s2, AML_NAME_LENGTH);
    while (end2 > s2 && *(end2 - 1) == '_')
    {
        end2--;
    }

    size_t len1 = end1 - s1;
    size_t len2 = end2 - s2;
    size_t minLen = (len1 < len2) ? len1 : len2;

    int cmp = memcmp(s1, s2, minLen);
    if (cmp != 0)
    {
        return false;
    }

    return len1 == len2;
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
        if (aml_is_name_equal(child->segment, name))
        {
            return child;
        }
    }

    errno = ENOENT;
    return NULL;
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

    // Pad with '_' characters.
    memset(node->segment, '_', AML_NAME_LENGTH);
    uint64_t nameLen = strnlen_s(name, AML_NAME_LENGTH);
    memcpy(node->segment, name, nameLen);
    node->segment[AML_NAME_LENGTH] = '\0';

    node->parent = parent;

    sysfs_dir_t* parentDir = NULL;
    char sysfsName[MAX_NAME];

    if (parent != NULL)
    {
        parentDir = &parent->dir;

        // Trim trailing '_' from the name.
        memset(sysfsName, 0, MAX_NAME);
        strncpy(sysfsName, name, AML_NAME_LENGTH);
        for (int64_t i = AML_NAME_LENGTH - 1; i >= 0; i--)
        {
            if (sysfsName[i] != '\0' && sysfsName[i] != '_')
            {
                break;
            }

            sysfsName[i] = '\0';
        }
    }
    else
    {
        assert(root == NULL && "Root node already exists");
        assert(strcmp(node->segment, AML_ROOT_NAME) == 0 && "Non root node has no parent");
        parentDir = acpi_get_sysfs_root();
        strcpy(sysfsName, "namespace");
    }

    if (sysfs_dir_init(&node->dir, parentDir, sysfsName, NULL, NULL) == ERR)
    {
        LOG_ERR("failed to create sysfs directory for AML node '%s'\n", sysfsName);
        heap_free(node);
        return NULL;
    }

    if (parent != NULL)
    {
        list_push(&parent->children, &node->entry);
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

    aml_node_t* current = start;
    if (start == NULL || string->rootChar.present)
    {
        current = aml_root_get();
    }

    for (uint64_t i = 0; i < string->prefixPath.depth; i++)
    {
        current = current->parent;
        if (current == NULL)
        {
            errno = ENOENT;
            return NULL;
        }
    }

    for (uint8_t i = 0; i < string->namePath.segmentCount - 1; i++)
    {
        const aml_name_seg_t* segment = &string->namePath.segments[i];
        current = aml_node_find_child(current, segment->name);
        if (current == NULL)
        {
            LOG_ERR("unable to find intermediate AML node '%.*s'\n", AML_NAME_LENGTH, segment->name);
            return NULL;
        }
    }

    const char* newNodeName = string->namePath.segments[string->namePath.segmentCount - 1].name;
    return aml_node_add(current, newNodeName, type);
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
    aml_node_t* current = start;

    switch (*ptr)
    {
    case AML_ROOT_CHAR:
        current = aml_root_get();
        ptr++;
        break;
    case AML_PARENT_PREFIX_CHAR:
        if (current == NULL)
        {
            errno = EINVAL;
            return NULL;
        }
        while (*ptr == AML_PARENT_PREFIX_CHAR)
        {
            current = current->parent;
            if (current == NULL)
            {
                errno = ENOENT;
                return NULL;
            }
            ptr++;
        }
        break;
    default:
        if (current == NULL)
        {
            current = aml_root_get();
        }
        break;
    }

    while (*ptr != '\0')
    {
        const char* segmentStart = ptr;
        while (*ptr != '.' && *ptr != '\0')
        {
            ptr++;
        }
        size_t segmentLength = ptr - segmentStart;

        if (segmentLength > AML_NAME_LENGTH)
        {
            errno = EILSEQ;
            return NULL;
        }

        char segment[AML_NAME_LENGTH + 1];
        memcpy(segment, segmentStart, segmentLength);
        segment[segmentLength] = '\0';

        current = aml_node_find_child(current, segment);
        if (current == NULL)
        {
            return NULL;
        }

        if (*ptr == '.')
        {
            ptr++;
        }
    }

    return current;
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
