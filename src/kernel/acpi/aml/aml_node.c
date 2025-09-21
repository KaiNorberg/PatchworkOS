#include "aml_node.h"

#include "acpi/acpi.h"
#include "aml_value.h"
#include "log/log.h"
#include "mem/heap.h"

#include "log/log.h"

#include <errno.h>
#include <stddef.h>
#include <sys/math.h>

static bool aml_is_name_equal(const char* s1, const char* s2)
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

aml_node_t* aml_node_new(aml_node_t* parent, const char* name, aml_node_type_t type)
{
    if (name == NULL || type < AML_NODE_NONE || type >= AML_NODE_MAX)
    {
        errno = EINVAL;
        return NULL;
    }

    MUTEX_SCOPE(aml_global_mutex_get());

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

    if (type == AML_NODE_ARG || type == AML_NODE_LOCAL)
    {
        assert(parent == NULL && "Args and Locals cannot have a parent");
        return node;
    }

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
        assert(aml_root_get() == NULL && "Root node already exists");
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

void aml_node_free(aml_node_t* node)
{
    if (node == NULL)
    {
        return;
    }

    MUTEX_SCOPE(aml_global_mutex_get());

    switch (node->type)
    {
    case AML_NODE_NAME:
        aml_data_object_deinit(&node->name.object);
        break;
    default:
        break;
    }

    aml_node_t* child = NULL;
    aml_node_t* temp = NULL;
    LIST_FOR_EACH_SAFE(child, temp, &node->children, entry)
    {
        list_remove(&node->children, &child->entry);
        aml_node_free(child);
    }

    sysfs_dir_deinit(&node->dir);
    heap_free(node);
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

aml_node_t* aml_node_add(aml_name_string_t* string, aml_node_t* start, aml_node_type_t type)
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
    return aml_node_new(current, newNodeName, type);
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
