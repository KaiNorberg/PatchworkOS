#include "aml_node.h"

#include "acpi/acpi.h"
#include "aml_to_string.h"
#include "aml_value.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"

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

aml_data_type_info_t* aml_data_type_get_info(aml_data_type_t type)
{
    static aml_data_type_info_t typeInfo[] = {
        {"Uninitialized", AML_DATA_UNINITALIZED, AML_DATA_FLAG_NONE},
        {"Buffer", AML_DATA_BUFFER, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"Buffer Field", AML_DATA_BUFFER_FIELD, AML_DATA_FLAG_DATA_OBJECT},
        {"Debug Object", AML_DATA_DEBUG_OBJECT, AML_DATA_FLAG_NONE},
        {"Device", AML_DATA_DEVICE, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Event", AML_DATA_EVENT, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Field Unit", AML_DATA_FIELD_UNIT, AML_DATA_FLAG_DATA_OBJECT},
        {"Integer", AML_DATA_INTEGER, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"Integer Constant", AML_DATA_INTEGER_CONSTANT, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"Method", AML_DATA_METHOD, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Mutex", AML_DATA_MUTEX, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Object Reference", AML_DATA_OBJECT_REFERENCE, AML_DATA_FLAG_NONE},
        {"Operation Region", AML_DATA_OPERATION_REGION, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Package", AML_DATA_PACKAGE, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"Power Resource", AML_DATA_POWER_RESOURCE, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Processor", AML_DATA_PROCESSOR, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Raw Data Buffer", AML_DATA_RAW_DATA_BUFFER, AML_DATA_FLAG_NONE},
        {"String", AML_DATA_STRING, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"Thermal Zone", AML_DATA_THERMAL_ZONE, AML_DATA_FLAG_NON_DATA_OBJECT},
    };
    static aml_data_type_info_t unknownType = {"Unknown", AML_DATA_UNINITALIZED, AML_DATA_FLAG_NONE};

    // Exactly one bit must be set, otherwise this does not make any sense.
    if (type == 0 || (type & (type - 1)) != 0)
    {
        return &unknownType;
    }

    // Use the set bit position as the index
    uint64_t index = __builtin_ffs(type) - 1;

    if (index < sizeof(typeInfo) / sizeof(typeInfo[0]) && typeInfo[index].type == type)
    {
        return &typeInfo[index];
    }

    return &unknownType;
}

aml_node_t* aml_node_new(aml_node_t* parent, const char* name, aml_node_flags_t flags)
{
    if (name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    uint64_t nameLen = strnlen_s(name, AML_NAME_LENGTH + 1);
    if (nameLen == 0 || nameLen > AML_NAME_LENGTH)
    {
        errno = EINVAL;
        return NULL;
    }

    MUTEX_SCOPE(aml_global_mutex_get());

    aml_node_t* node = heap_alloc(sizeof(aml_node_t), HEAP_NONE);
    if (node == NULL)
    {
        return NULL;
    }

    list_entry_init(&node->entry);
    node->type = AML_DATA_UNINITALIZED;
    node->flags = flags;
    list_init(&node->children);
    node->parent = parent;

    // Pad with '_' characters.
    memset(node->segment, '_', AML_NAME_LENGTH);
    memcpy(node->segment, name, nameLen);
    node->segment[AML_NAME_LENGTH] = '\0';

    mutex_init(&node->lock);
    node->isAllocated = false;
    memset(&node->dir, 0, sizeof(sysfs_dir_t));

    sysfs_dir_t* parentDir = NULL;
    char sysfsName[MAX_NAME];

    if (parent != NULL)
    {
        if (flags & AML_NODE_ROOT)
        {
            LOG_ERR("Non root node cannot have the AML_NODE_ROOT flag set\n");
            aml_node_free(node);
            errno = EINVAL;
            return NULL;
        }

        if (flags & AML_NODE_DISCONNECTED)
        {
            LOG_ERR("Node cannot have a parent and be disconnected\n");
            aml_node_free(node);
            errno = EINVAL;
            return NULL;
        }

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
    else if (flags & AML_NODE_ROOT)
    {
        assert(aml_root_get() == NULL && "Root node already exists");
        assert(strcmp(node->segment, AML_ROOT_NAME) == 0 && "Non root node has no parent");
        parentDir = acpi_get_sysfs_root();
        strcpy(sysfsName, "namespace");
    }
    else
    {
        if (!(flags & AML_NODE_DISCONNECTED))
        {
            LOG_ERR("Disconnected nodes cant have a parent\n");
            aml_node_free(node);
            errno = EINVAL;
            return NULL;
        }
        return node;
    }

    if (parent != NULL && aml_node_find_child(parent, node->segment) != NULL)
    {
        LOG_ERR("AML node '%s' already exists under parent '%s'\n", node->segment,
            parent != NULL ? parent->segment : "NULL");
        aml_node_free(node);
        errno = EEXIST;
        return NULL;
    }

    if (sysfs_dir_init(&node->dir, parentDir, sysfsName, NULL, NULL) == ERR)
    {
        LOG_ERR("failed to create sysfs directory for AML node '%s'\n", sysfsName);
        aml_node_free(node);
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

    if (!node->isAllocated)
    {
        panic(NULL, "Attempted to free a node that was not allocated\n");
    }

    mutex_t* globalMutex = aml_global_mutex_get();

    if (node->parent != NULL)
    {
        mutex_acquire_recursive(globalMutex);
        list_remove(&node->parent->children, &node->entry);
        mutex_release(globalMutex);
    }

    aml_node_deinit(node);

    aml_node_t* child = NULL;
    aml_node_t* temp = NULL;
    LIST_FOR_EACH_SAFE(child, temp, &node->children, entry)
    {
        mutex_acquire_recursive(globalMutex);
        list_remove(&node->children, &child->entry);
        mutex_release(globalMutex);

        aml_node_free(child);
    }

    heap_free(node);
}

uint64_t aml_node_init_buffer(aml_node_t* node, uint8_t* buffer, uint64_t length, uint64_t capacity, bool inPlace)
{
    if (node == NULL || buffer == NULL || capacity == 0 || length > capacity)
    {
        errno = EINVAL;
        return ERR;
    }

    mutex_acquire_recursive(&node->lock);

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_BUFFER;
    if (inPlace)
    {
        node->buffer.content = buffer;
        node->buffer.length = length;
        node->buffer.capacity = capacity;
        node->buffer.inPlace = true;
    }
    else
    {
        node->buffer.content = heap_alloc(capacity, HEAP_NONE);
        if (node->buffer.content == NULL)
        {
            mutex_release(&node->lock);
            return ERR;
        }
        memcpy(node->buffer.content, buffer, length);
        memset(node->buffer.content + length, 0, capacity - length);
        node->buffer.length = length;
        node->buffer.capacity = capacity;
        node->buffer.inPlace = false;
    }

    mutex_release(&node->lock);
    return 0;
}

uint64_t aml_node_init_device(aml_node_t* node)
{
    if (node == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mutex_acquire_recursive(&node->lock);

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_DEVICE;
    memset(&node->device, 0, sizeof(node->device));

    mutex_release(&node->lock);
    return 0;
}

uint64_t aml_node_init_integer(aml_node_t* node, uint64_t value, uint8_t bitWidth)
{
    if (node == NULL || (bitWidth != 8 && bitWidth != 16 && bitWidth != 32 && bitWidth != 64))
    {
        errno = EINVAL;
        return ERR;
    }

    mutex_acquire_recursive(&node->lock);

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_INTEGER;
    node->integer.value = value;
    node->integer.bitWidth = bitWidth;

    mutex_release(&node->lock);
    return 0;
}

uint64_t aml_node_init_integer_constant(aml_node_t* node, uint64_t value)
{
    if (node == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mutex_acquire_recursive(&node->lock);

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_INTEGER_CONSTANT;
    node->integerConstant.value = value;

    mutex_release(&node->lock);
    return 0;
}

uint64_t aml_node_init_method(aml_node_t* node, aml_method_flags_t flags, aml_address_t start, aml_address_t end)
{
    if (node == NULL || start == 0 || end == 0 || start > end)
    {
        errno = EINVAL;
        return ERR;
    }

    mutex_acquire_recursive(&node->lock);

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_METHOD;
    node->method.flags = flags;
    node->method.start = start;
    node->method.end = end;

    mutex_release(&node->lock);
    return 0;
}

uint64_t aml_node_init_object_reference(aml_node_t* node, aml_node_t* target)
{
    if (node == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mutex_acquire_recursive(&node->lock);

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_OBJECT_REFERENCE;
    node->objectReference.target = target;

    mutex_release(&node->lock);
    return 0;
}

uint64_t aml_node_init_package(aml_node_t* node, uint64_t capacity)
{
    if (node == NULL || capacity == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    mutex_acquire_recursive(&node->lock);

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_PACKAGE;
    node->package.capacity = capacity;
    node->package.elements = heap_alloc(sizeof(aml_node_t) * capacity, HEAP_NONE);
    if (node->package.elements == NULL)
    {
        mutex_release(&node->lock);
        return ERR;
    }

    for (uint64_t i = 0; i < capacity; i++)
    {
        node->package.elements[i] = aml_node_new(NULL, "____", AML_NODE_DISCONNECTED);
        if (node->package.elements[i] == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_free(node->package.elements[j]);
            }
            heap_free(node->package.elements);
            node->package.elements = NULL;
            mutex_release(&node->lock);
            return ERR;
        }
    }

    mutex_release(&node->lock);
    return 0;
}

uint64_t aml_node_init_string(aml_node_t* node, const char* str, bool inPlace)
{
    if (node == NULL || str == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mutex_acquire_recursive(&node->lock);

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_STRING;
    if (inPlace)
    {
        node->string.content = (char*)str;
        node->string.inPlace = true;
    }
    else
    {
        uint64_t strLen = strlen(str);
        node->string.content = heap_alloc(strLen + 1, HEAP_NONE);
        if (node->string.content == NULL)
        {
            mutex_release(&node->lock);
            return ERR;
        }
        memcpy(node->string.content, str, strLen);
        node->string.content[strLen] = '\0';
        node->string.inPlace = false;
    }

    mutex_release(&node->lock);
    return 0;
}

void aml_node_deinit(aml_node_t* node)
{
    if (node == NULL)
    {
        return;
    }

    mutex_acquire_recursive(&node->lock);

    switch (node->type)
    {
    case AML_DATA_UNINITALIZED:
        // Nothing to do.
        break;
    case AML_DATA_BUFFER:
        if (!node->buffer.inPlace && node->buffer.content != NULL)
        {
            heap_free(node->buffer.content);
        }
        node->buffer.length = 0;
        node->buffer.capacity = 0;
        node->buffer.content = NULL;
        break;
    case AML_DATA_DEVICE:
    case AML_DATA_INTEGER:
    case AML_DATA_INTEGER_CONSTANT:
        // Nothing to do.
        break;
    case AML_DATA_STRING:
        if (!node->string.inPlace && node->string.content != NULL)
        {
            heap_free(node->string.content);
        }
        node->string.content = NULL;
        break;
    case AML_DATA_PACKAGE:
        if (node->package.elements != NULL)
        {
            for (uint64_t i = 0; i < node->package.capacity; i++)
            {
                aml_node_free(node->package.elements[i]);
            }
            heap_free(node->package.elements);
        }
        node->package.capacity = 0;
        node->package.elements = NULL;
        break;
    default:
        panic(NULL, "unimplemented deinit of AML node '%.*s' of type '%s'\n", AML_NAME_LENGTH, node->segment,
            aml_node_type_to_string(node->type));
    }

    node->type = AML_DATA_UNINITALIZED;

    mutex_release(&node->lock);
}

uint64_t aml_node_clone(aml_node_t* src, aml_node_t* dest)
{
    if (src == NULL || dest == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (src == dest)
    {
        return 0;
    }

    mutex_acquire_recursive(&src->lock);
    mutex_acquire_recursive(&dest->lock);

    if (dest->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(dest);
    }

    dest->type = src->type;

    switch (src->type)
    {
    default:
        panic(NULL, "unimplemented clone of AML node '%.*s' of type '%s'\n", AML_NAME_LENGTH, src->segment,
            aml_node_type_to_string(src->type));
        break;
    }

    mutex_release(&dest->lock);
    mutex_release(&src->lock);
    return 0;
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

aml_node_t* aml_node_add(aml_name_string_t* string, aml_node_t* start, aml_data_type_t type, aml_node_flags_t flags)
{
    if (string == NULL || type == 0 || !(type & AML_DATA_ALL) || (flags & AML_NODE_ROOT))
    {
        errno = EINVAL;
        return NULL;
    }

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
    return aml_node_new(current, newNodeName, type, flags);
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
            if (start->parent != NULL)
            {
                return aml_node_find(nameString, start->parent);
            }
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
