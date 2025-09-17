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
    memcpy(node->name, name, AML_NAME_LENGTH);
    node->name[AML_NAME_LENGTH] = '\0';

    if (parent != NULL)
    {
        if (parent->parent == NULL)
        {
            assert(root == parent);
        }

        if (sysfs_dir_init(&node->dir, &parent->dir, node->name, NULL, NULL) == ERR)
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
        assert(strcmp(node->name, AML_ROOT_NAME) == 0);

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
            if (memcmp(child->name, segment->name, AML_NAME_LENGTH) == 0)
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

aml_node_t* aml_node_find(const aml_name_string_t* nameString, aml_node_t* start)
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

bool aml_should_acquire_global_mutex(aml_node_t* node)
{
    if (node == NULL)
    {
        return false;
    }

    switch (node->type)
    {
    case AML_NODE_FIELD:
        return node->data.field.flags.lockRule == AML_LOCK_RULE_LOCK;
    case AML_NODE_INDEX_FIELD:
        return node->data.indexField.flags.lockRule == AML_LOCK_RULE_LOCK;
    default:
        return false;
    }
}

void aml_align_bits(aml_bit_size_t bits, aml_access_type_t accessType, aml_bit_size_t* out, aml_bit_size_t* remainder)
{
    if (out == NULL || remainder == NULL)
    {
        return;
    }

    aml_bit_size_t alignSize = 1;
    switch (accessType)
    {
    case AML_ACCESS_TYPE_BYTE:
        alignSize = 8;
        break;
    case AML_ACCESS_TYPE_WORD:
        alignSize = 16;
        break;
    case AML_ACCESS_TYPE_DWORD:
        alignSize = 32;
        break;
    case AML_ACCESS_TYPE_QWORD:
        alignSize = 64;
        break;
    default:
        *out = bits;
        *remainder = 0;
        return;
    }

    *out = bits - (bits % alignSize);
    *remainder = bits % alignSize;
}

uint64_t aml_node_evaluate(aml_node_t* node, aml_data_object_t* out, aml_term_arg_list_t* args)
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
        aml_align_bits(node->data.indexField.bitOffset, node->data.indexField.flags.accessType, &alignedBits, &remainder);

        uint64_t byteOffset = alignedBits / 8;

        // "The value written to the IndexName register is defined to be a byte offset that is aligned on an AccessType
        // boundary." - Section 19.6.64
        // Honestly just good luck with the alignment stuff.
        aml_data_object_t index = AML_DATA_OBJECT_INTEGER(byteOffset);
        if (aml_node_store(node->data.indexField.indexNode, &index) == ERR)
        {
            result = ERR;
            break;
        }

        aml_data_object_t data;
        if (aml_node_evaluate(node->data.indexField.dataNode, &data, NULL) == ERR)
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

uint64_t aml_node_store(aml_node_t* node, aml_data_object_t* object)
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
        // Genuinly just good luck, field access is a... thing.

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
            aml_update_rule_to_string(node->data.field.flags.updateRule), node->data.field.bitOffset,
            node->data.field.bitSize);
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
