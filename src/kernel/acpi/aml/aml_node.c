#include "aml_node.h"

#include "acpi/acpi.h"
#include "aml.h"
#include "aml_to_string.h"
#include "aml_value.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "sync/mutex.h"

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
        {"BufferField", AML_DATA_BUFFER_FIELD, AML_DATA_FLAG_DATA_OBJECT},
        {"DebugObject", AML_DATA_DEBUG_OBJECT, AML_DATA_FLAG_NONE},
        {"Device", AML_DATA_DEVICE, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Event", AML_DATA_EVENT, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"FieldUnit", AML_DATA_FIELD_UNIT, AML_DATA_FLAG_DATA_OBJECT},
        {"Integer", AML_DATA_INTEGER, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"Integer Constant", AML_DATA_INTEGER_CONSTANT, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"Method", AML_DATA_METHOD, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Mutex", AML_DATA_MUTEX, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"ObjectReference", AML_DATA_OBJECT_REFERENCE, AML_DATA_FLAG_NONE},
        {"OperationRegion", AML_DATA_OPERATION_REGION, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Package", AML_DATA_PACKAGE, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"PowerResource", AML_DATA_POWER_RESOURCE, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Processor", AML_DATA_PROCESSOR, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"RawDataBuffer", AML_DATA_RAW_DATA_BUFFER, AML_DATA_FLAG_NONE},
        {"String", AML_DATA_STRING, AML_DATA_FLAG_DATA_OBJECT | AML_DATA_FLAG_IS_ACTUAL_DATA},
        {"Thermal Zone", AML_DATA_THERMAL_ZONE, AML_DATA_FLAG_NON_DATA_OBJECT},
        {"Unresolved", AML_DATA_UNRESOLVED, AML_DATA_FLAG_NONE},
    };
    static aml_data_type_info_t unknownType = {"Unknown", AML_DATA_UNINITALIZED, AML_DATA_FLAG_NONE};

    for (size_t i = 0; i < sizeof(typeInfo) / sizeof(typeInfo[0]); i++)
    {
        if (typeInfo[i].type == type)
        {
            return &typeInfo[i];
        }
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

    uint64_t nameLen = strnlen_s(name, AML_NAME_LENGTH);
    if (nameLen == 0)
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

    node->isAllocated = true;
    memset(&node->dir, 0, sizeof(sysfs_dir_t));

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
    else if (flags & AML_NODE_ROOT)
    {
        assert(aml_root_get() == NULL && "Root node already exists");
        assert(strcmp(node->segment, AML_ROOT_NAME) == 0 && "Non root node has no parent");
        parentDir = acpi_get_sysfs_root();
        strcpy(sysfsName, "namespace");
    }
    else
    {
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
        panic(NULL, "Attempted to free a node that was not allocated");
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

aml_node_t* aml_node_add(aml_name_string_t* string, aml_node_t* start, aml_node_flags_t flags)
{
    if (string == NULL || (flags & AML_NODE_ROOT))
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

    char newNodeName[AML_NAME_LENGTH + 1];
    memcpy(newNodeName, string->namePath.segments[string->namePath.segmentCount - 1].name, AML_NAME_LENGTH);
    newNodeName[AML_NAME_LENGTH] = '\0';

    return aml_node_new(current, newNodeName, flags);
}

uint64_t aml_node_init_buffer(aml_node_t* node, const uint8_t* buffer, uint64_t bytesToCopy, uint64_t length)
{
    if (node == NULL || buffer == NULL || length == 0 || bytesToCopy > length)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_BUFFER;
    node->buffer.content = heap_alloc(length, HEAP_NONE);
    if (node->buffer.content == NULL)
    {
        return ERR;
    }
    memset(node->buffer.content, 0, length);
    memcpy(node->buffer.content, buffer, bytesToCopy);
    node->buffer.length = length;

    node->buffer.byteFields = heap_alloc(sizeof(aml_node_t) * length, HEAP_NONE);
    if (node->buffer.byteFields == NULL)
    {
        heap_free(node->buffer.content);
        node->buffer.content = NULL;
        return ERR;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        node->buffer.byteFields[i] = AML_NODE_CREATE;
        if (aml_node_init_buffer_field(&node->buffer.byteFields[i], node->buffer.content, i * 8, 8) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_deinit(&node->buffer.byteFields[j]);
            }
            heap_free(node->buffer.byteFields);
            node->buffer.byteFields = NULL;
            heap_free(node->buffer.content);
            node->buffer.content = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_node_init_buffer_empty(aml_node_t* node, uint64_t length)
{
    if (node == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_BUFFER;
    node->buffer.content = heap_alloc(length, HEAP_NONE);
    if (node->buffer.content == NULL)
    {
        return ERR;
    }
    memset(node->buffer.content, 0, length);
    node->buffer.length = length;

    for (uint64_t i = 0; i < length; i++)
    {
        node->buffer.byteFields[i] = AML_NODE_CREATE;
        if (aml_node_init_buffer_field(&node->buffer.byteFields[i], node->buffer.content, i * 8, 8) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_deinit(&node->buffer.byteFields[j]);
            }
            heap_free(node->buffer.byteFields);
            node->buffer.byteFields = NULL;
            heap_free(node->buffer.content);
            node->buffer.content = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_node_init_buffer_field(aml_node_t* node, uint8_t* buffer, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (node == NULL || buffer == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_BUFFER_FIELD;
    node->bufferField.buffer = buffer;
    node->bufferField.bitOffset = bitOffset;
    node->bufferField.bitSize = bitSize;

    return 0;
}

uint64_t aml_node_init_device(aml_node_t* node)
{
    if (node == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_DEVICE;
    memset(&node->device, 0, sizeof(node->device));

    return 0;
}

uint64_t aml_node_init_field_unit_field(aml_node_t* node, aml_node_t* opregion, aml_field_flags_t flags,
    aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (node == NULL || opregion == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_FIELD_UNIT;
    node->fieldUnit.type = AML_FIELD_UNIT_FIELD;
    node->fieldUnit.opregion = opregion;
    node->fieldUnit.flags = flags;
    node->fieldUnit.bitOffset = bitOffset;
    node->fieldUnit.bitSize = bitSize;
    node->fieldUnit.regionSpace = opregion->opregion.space;

    return 0;
}

uint64_t aml_node_init_field_unit_index_field(aml_node_t* node, aml_node_t* indexNode, aml_node_t* dataNode,
    aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (node == NULL || indexNode == NULL || dataNode == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_FIELD_UNIT;
    node->fieldUnit.type = AML_FIELD_UNIT_INDEX_FIELD;
    node->fieldUnit.indexNode = indexNode;
    node->fieldUnit.dataNode = dataNode;
    node->fieldUnit.flags = flags;
    node->fieldUnit.bitOffset = bitOffset;
    node->fieldUnit.bitSize = bitSize;
    node->fieldUnit.regionSpace = dataNode->fieldUnit.regionSpace;

    return 0;
}

uint64_t aml_node_init_field_unit_bank_field(aml_node_t* node, aml_node_t* opregion, aml_node_t* bank,
    aml_qword_data_t bankValue, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (node == NULL || opregion == NULL || bank == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_FIELD_UNIT;
    node->fieldUnit.type = AML_FIELD_UNIT_BANK_FIELD;
    node->fieldUnit.opregion = opregion;
    node->fieldUnit.bank = bank;
    node->fieldUnit.bankValue = bankValue;
    node->fieldUnit.flags = flags;
    node->fieldUnit.bitOffset = bitOffset;
    node->fieldUnit.bitSize = bitSize;
    node->fieldUnit.regionSpace = opregion->opregion.space;

    return 0;
}

uint64_t aml_node_init_integer(aml_node_t* node, uint64_t value)
{
    if (node == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_INTEGER;
    node->integer.value = value;

    return 0;
}

uint64_t aml_node_init_integer_constant(aml_node_t* node, uint64_t value)
{
    if (node == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_INTEGER_CONSTANT;
    node->integerConstant.value = value;

    return 0;
}

uint64_t aml_node_init_method(aml_node_t* node, aml_method_flags_t* flags, const uint8_t* start, const uint8_t* end,
    aml_method_implementation_t implementation)
{
    if (node == NULL || ((start == 0 || end == 0 || start > end) && implementation == NULL))
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_METHOD;
    node->method.implementation = implementation;
    node->method.flags = *flags;
    node->method.start = start;
    node->method.end = end;

    return 0;
}

uint64_t aml_node_init_mutex(aml_node_t* node, aml_sync_level_t syncLevel)
{
    if (node == NULL || syncLevel > 15)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_MUTEX;
    node->mutex.syncLevel = syncLevel;
    mutex_init(&node->mutex.mutex);

    return 0;
}

uint64_t aml_node_init_object_reference(aml_node_t* node, aml_node_t* target)
{
    if (node == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_OBJECT_REFERENCE;
    node->objectReference.target = target;

    return 0;
}

uint64_t aml_node_init_opregion(aml_node_t* node, aml_region_space_t space, uint64_t offset, uint32_t length)
{
    if (node == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_OPERATION_REGION;
    node->opregion.space = space;
    node->opregion.offset = offset;
    node->opregion.length = length;

    return 0;
}

uint64_t aml_node_init_package(aml_node_t* node, uint64_t length)
{
    if (node == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_PACKAGE;

    if (length == 0)
    {
        node->package.length = 0;
        node->package.elements = NULL;
        return 0;
    }

    node->package.length = length;
    node->package.elements = heap_alloc(sizeof(aml_node_t) * length, HEAP_NONE);
    if (node->package.elements == NULL)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        node->package.elements[i] = aml_node_new(NULL, "____", AML_NODE_NONE);
        if (node->package.elements[i] == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_free(node->package.elements[j]);
            }
            heap_free(node->package.elements);
            node->package.elements = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_node_init_processor(aml_node_t* node, aml_proc_id_t procId, aml_pblk_addr_t pblkAddr,
    aml_pblk_len_t pblkLen)
{
    if (node == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_PROCESSOR;
    node->processor.procId = procId;
    node->processor.pblkAddr = pblkAddr;
    node->processor.pblkLen = pblkLen;

    return 0;
}

uint64_t aml_node_init_string(aml_node_t* node, const char* str)
{
    if (node == NULL || str == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_STRING;
    uint64_t strLen = strlen(str);

    if (strLen == 0)
    {
        node->string.content = heap_alloc(1, HEAP_NONE);
        if (node->string.content == NULL)
        {
            return ERR;
        }
        node->string.content[0] = '\0';
        node->string.length = 0;
        node->string.byteFields = NULL;
        return 0;
    }

    node->string.content = heap_alloc(strLen + 1, HEAP_NONE);
    if (node->string.content == NULL)
    {
        return ERR;
    }
    node->string.length = strLen;
    memcpy(node->string.content, str, strLen);
    node->string.content[strLen] = '\0';

    node->string.byteFields = heap_alloc(sizeof(aml_node_t) * strLen, HEAP_NONE);
    if (node->string.byteFields == NULL)
    {
        heap_free(node->string.content);
        node->string.content = NULL;
        return ERR;
    }

    for (uint64_t i = 0; i < strLen; i++)
    {
        node->string.byteFields[i] = AML_NODE_CREATE;
        if (aml_node_init_buffer_field(&node->string.byteFields[i], (uint8_t*)node->string.content, i * 8, 8) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_node_deinit(&node->string.byteFields[j]);
            }
            heap_free(node->string.content);
            node->string.content = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_node_init_unresolved(aml_node_t* node, aml_name_string_t* nameString, aml_node_t* start)
{
    if (node == NULL || start == NULL || nameString == NULL || nameString->namePath.segmentCount == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_UNRESOLVED;
    node->unresolved.nameString = *nameString;
    node->unresolved.start = start;

    return 0;
}

uint64_t aml_node_init_alias(aml_node_t* node, aml_node_t* target)
{
    if (node == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (node->type != AML_DATA_UNINITALIZED)
    {
        aml_node_deinit(node);
    }

    node->type = AML_DATA_ALIAS;
    node->alias.target = target;

    return 0;
}

void aml_node_deinit(aml_node_t* node)
{
    if (node == NULL)
    {
        return;
    }

    switch (node->type)
    {
    case AML_DATA_UNINITALIZED:
    case AML_DATA_BUFFER_FIELD:
    case AML_DATA_DEVICE:
    case AML_DATA_INTEGER:
    case AML_DATA_INTEGER_CONSTANT:
    case AML_DATA_METHOD:
    case AML_DATA_OBJECT_REFERENCE:
    case AML_DATA_OPERATION_REGION:
        // Nothing to do.
        break;
    case AML_DATA_BUFFER:
        if (node->buffer.byteFields != NULL)
        {
            for (uint64_t i = 0; i < node->buffer.length; i++)
            {
                aml_node_deinit(&node->buffer.byteFields[i]);
            }
            heap_free(node->buffer.byteFields);
        }
        if (node->buffer.content != NULL)
        {
            heap_free(node->buffer.content);
        }
        node->buffer.length = 0;
        node->buffer.content = NULL;
        node->buffer.byteFields = NULL;
        break;
    case AML_DATA_MUTEX:
        mutex_deinit(&node->mutex.mutex);
        break;
    case AML_DATA_PACKAGE:
        if (node->package.elements != NULL)
        {
            for (uint64_t i = 0; i < node->package.length; i++)
            {
                aml_node_free(node->package.elements[i]);
            }
            heap_free(node->package.elements);
        }
        node->package.length = 0;
        node->package.elements = NULL;
        break;
    case AML_DATA_STRING:
        if (node->string.byteFields != NULL)
        {
            for (uint64_t i = 0; i < node->string.length; i++)
            {
                aml_node_deinit(&node->string.byteFields[i]);
            }
            heap_free(node->string.byteFields);
        }
        if (node->string.content != NULL)
        {
            heap_free(node->string.content);
        }
        node->string.length = 0;
        node->string.content = NULL;
        break;
    case AML_DATA_UNRESOLVED:
        aml_patch_up_remove_unresolved(node);
        break;
    default:
        panic(NULL, "unimplemented deinit of AML node '%.*s' of type '%s'\n", AML_NAME_LENGTH, node->segment,
            aml_data_type_to_string(node->type));
    }

    node->type = AML_DATA_UNINITALIZED;
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

    switch (src->type)
    {
    case AML_DATA_BUFFER:
        if (aml_node_init_buffer(dest, src->buffer.content, src->buffer.length, src->buffer.length) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_BUFFER_FIELD:
        if (aml_node_init_buffer_field(dest, src->bufferField.buffer, src->bufferField.bitOffset,
                src->bufferField.bitSize) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_DEVICE:
        if (aml_node_init_device(dest) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_FIELD_UNIT:
        if (src->fieldUnit.type == AML_FIELD_UNIT_FIELD)
        {
            if (aml_node_init_field_unit_field(dest, src->fieldUnit.opregion, src->fieldUnit.flags,
                    src->fieldUnit.bitOffset, src->fieldUnit.bitSize) == ERR)
            {
                return ERR;
            }
        }
        else if (src->fieldUnit.type == AML_FIELD_UNIT_INDEX_FIELD)
        {
            if (aml_node_init_field_unit_index_field(dest, src->fieldUnit.indexNode, src->fieldUnit.dataNode,
                    src->fieldUnit.flags, src->fieldUnit.bitOffset, src->fieldUnit.bitSize) == ERR)
            {
                return ERR;
            }
        }
        else if (src->fieldUnit.type == AML_FIELD_UNIT_BANK_FIELD)
        {
            if (aml_node_init_field_unit_bank_field(dest, src->fieldUnit.opregion, src->fieldUnit.bank,
                    src->fieldUnit.bankValue, src->fieldUnit.flags, src->fieldUnit.bitOffset,
                    src->fieldUnit.bitSize) == ERR)
            {
                return ERR;
            }
        }
        else
        {
            errno = EINVAL;
            return ERR;
        }
        break;
    case AML_DATA_INTEGER:
        if (aml_node_init_integer(dest, src->integer.value) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_INTEGER_CONSTANT:
        if (aml_node_init_integer_constant(dest, src->integerConstant.value) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_METHOD:
        if (aml_node_init_method(dest, &src->method.flags, src->method.start, src->method.end,
                src->method.implementation) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_MUTEX:
        if (aml_node_init_mutex(dest, src->mutex.syncLevel) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_OBJECT_REFERENCE:
        if (aml_node_init_object_reference(dest, src->objectReference.target) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_OPERATION_REGION:
        if (aml_node_init_opregion(dest, src->opregion.space, src->opregion.offset, src->opregion.length) == ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_PACKAGE:
        if (aml_node_init_package(dest, src->package.length) == ERR)
        {
            return ERR;
        }
        for (uint64_t i = 0; i < src->package.length; i++)
        {
            if (aml_node_clone(src->package.elements[i], dest->package.elements[i]) == ERR)
            {
                aml_node_deinit(dest);
                return ERR;
            }
        }
        break;
    case AML_DATA_PROCESSOR:
        if (aml_node_init_processor(dest, src->processor.procId, src->processor.pblkAddr, src->processor.pblkLen) ==
            ERR)
        {
            return ERR;
        }
        break;
    case AML_DATA_STRING:
        if (aml_node_init_string(dest, src->string.content) == ERR)
        {
            return ERR;
        }
        break;
    default:
        LOG_ERR("unimplemented clone of AML node '%.*s' of type ('%s', %d)\n", AML_NAME_LENGTH, src->segment,
            aml_data_type_to_string(src->type), src->type);
        errno = ENOSYS;
        return ERR;
    }

    return 0;
}

aml_node_t* aml_node_traverse_alias(aml_node_t* node)
{
    while (node != NULL && node->type == AML_DATA_ALIAS)
    {
        node = node->alias.target;
    }
    return node;
}

aml_node_t* aml_node_find_child(aml_node_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    if (parent->type == AML_DATA_ALIAS)
    {
        parent = parent->alias.target;
    }

    aml_node_t* child = NULL;
    LIST_FOR_EACH(child, &parent->children, entry)
    {
        if (aml_is_name_equal(child->segment, name))
        {
            if (child->type == AML_DATA_ALIAS)
            {
                return aml_node_traverse_alias(child);
            }
            return child;
        }
    }

    errno = ENOENT;
    return NULL;
}

aml_node_t* aml_node_find(const char* path, aml_node_t* start)
{
    if (path == NULL || path[0] == '\0')
    {
        errno = EINVAL;
        return NULL;
    }

    if (start->type == AML_DATA_ALIAS)
    {
        start = aml_node_traverse_alias(start);
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
uint64_t aml_node_put_bits_at(aml_node_t* node, uint64_t value, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (node == NULL || bitSize == 0 || bitSize > AML_INTEGER_BIT_WIDTH)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (node->type)
    {
    case AML_DATA_INTEGER:
        if (bitOffset + bitSize > AML_INTEGER_BIT_WIDTH)
        {
            errno = EINVAL;
            return ERR;
        }

        uint64_t mask;
        if (bitSize == 64)
        {
            mask = ~UINT64_C(0);
        }
        else
        {
            mask = (UINT64_C(1) << bitSize) - 1;
        }

        node->integer.value &= ~(mask << bitOffset);
        node->integer.value |= (value & mask) << bitOffset;
        break;
    case AML_DATA_BUFFER:
        if (bitOffset + bitSize > node->buffer.length * 8)
        {
            errno = EINVAL;
            return ERR;
        }
        for (aml_bit_size_t i = 0; i < bitSize; i++) // TODO: Optimize
        {
            aml_bit_size_t totalBitPos = bitOffset + i;
            aml_bit_size_t bytePos = totalBitPos / 8;
            aml_bit_size_t bitPos = totalBitPos % 8;

            if (value & (UINT64_C(1) << i))
            {
                node->buffer.content[bytePos] |= (1 << bitPos);
            }
            else
            {
                node->buffer.content[bytePos] &= ~(1 << bitPos);
            }
        }
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

uint64_t aml_node_get_bits_at(aml_node_t* node, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint64_t* out)
{
    if (node == NULL || out == NULL || bitSize == 0 || bitSize > 64)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (node->type)
    {
    case AML_DATA_INTEGER:
        if (bitOffset + bitSize > AML_INTEGER_BIT_WIDTH)
        {
            errno = EINVAL;
            return ERR;
        }

        uint64_t mask;
        if (bitSize == 64)
        {
            mask = ~UINT64_C(0);
        }
        else
        {
            mask = (UINT64_C(1) << bitSize) - 1;
        }

        *out = (node->integer.value >> bitOffset) & mask;
        break;
    case AML_DATA_BUFFER:
        if (bitOffset + bitSize > node->buffer.length * 8)
        {
            errno = EINVAL;
            return ERR;
        }
        *out = 0;
        for (aml_bit_size_t i = 0; i < bitSize; i++) // TODO: Optimize
        {
            aml_bit_size_t totalBitPos = bitOffset + i;
            aml_bit_size_t bytePos = totalBitPos / 8;
            aml_bit_size_t bitPos = totalBitPos % 8;

            if (node->buffer.content[bytePos] & (1 << bitPos))
            {
                *out |= (UINT64_C(1) << i);
            }
        }
        break;
    case AML_DATA_STRING:
        if (bitOffset + bitSize > strlen(node->string.content) * 8)
        {
            errno = EINVAL;
            return ERR;
        }
        *out = 0;
        for (aml_bit_size_t i = 0; i < bitSize; i++) // TODO: Optimize
        {
            aml_bit_size_t totalBitPos = bitOffset + i;
            aml_bit_size_t bytePos = totalBitPos / 8;
            aml_bit_size_t bitPos = totalBitPos % 8;

            if (node->string.content[bytePos] & (1 << bitPos))
            {
                *out |= (UINT64_C(1) << i);
            }
        }
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}
