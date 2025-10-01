#include "aml_object.h"

#include "acpi/acpi.h"
#include "aml.h"
#include "aml_to_string.h"
#include "aml_token.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "runtime/convert.h"
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

aml_object_t* aml_object_new(aml_object_t* parent, const char* name, aml_object_flags_t flags)
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

    aml_object_t* object = heap_alloc(sizeof(aml_object_t), HEAP_NONE);
    if (object == NULL)
    {
        return NULL;
    }

    list_entry_init(&object->entry);
    object->type = AML_DATA_UNINITALIZED;
    object->flags = flags;
    list_init(&object->children);
    object->parent = parent;

    // Pad with '_' characters.
    memset(object->segment, '_', AML_NAME_LENGTH);
    memcpy(object->segment, name, nameLen);
    object->segment[AML_NAME_LENGTH] = '\0';

    object->isAllocated = true;
    memset(&object->dir, 0, sizeof(sysfs_dir_t));

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
    else if (flags & AML_OBJECT_ROOT)
    {
        assert(aml_root_get() == NULL && "Root object already exists");
        assert(strcmp(object->segment, AML_ROOT_NAME) == 0 && "Non root object has no parent");
        parentDir = acpi_get_sysfs_root();
        strcpy(sysfsName, "namespace");
    }
    else // If not root dont add to sysfs.
    {
        return object;
    }

    object->flags |= AML_OBJECT_NAMED;

    if (parent != NULL && aml_object_find_child(parent, object->segment) != NULL)
    {
        LOG_ERR("AML object '%s' already exists under parent '%s'\n", object->segment,
            parent != NULL ? parent->segment : "NULL");
        aml_object_free(object);
        errno = EEXIST;
        return NULL;
    }

    if (sysfs_dir_init(&object->dir, parentDir, sysfsName, NULL, NULL) == ERR)
    {
        LOG_ERR("failed to create sysfs directory for AML object '%s'\n", sysfsName);
        aml_object_free(object);
        return NULL;
    }

    if (parent != NULL)
    {
        list_push(&parent->children, &object->entry);
    }

    return object;
}

void aml_object_free(aml_object_t* object)
{
    if (object == NULL)
    {
        return;
    }

    if (!object->isAllocated)
    {
        panic(NULL, "Attempted to free a object that was not allocated");
    }

    mutex_t* globalMutex = aml_global_mutex_get();

    if (object->parent != NULL)
    {
        mutex_acquire_recursive(globalMutex);
        list_remove(&object->parent->children, &object->entry);
        mutex_release(globalMutex);
    }

    aml_object_deinit(object);

    aml_object_t* child = NULL;
    aml_object_t* temp = NULL;
    LIST_FOR_EACH_SAFE(child, temp, &object->children, entry)
    {
        mutex_acquire_recursive(globalMutex);
        list_remove(&object->children, &child->entry);
        mutex_release(globalMutex);

        aml_object_free(child);
    }

    heap_free(object);
}

aml_object_t* aml_object_add(aml_object_t* start, aml_name_string_t* string, aml_object_flags_t flags)
{
    if (string == NULL || (flags & AML_OBJECT_ROOT))
    {
        errno = EINVAL;
        return NULL;
    }

    if (string->namePath.segmentCount == 0)
    {
        errno = EILSEQ;
        return NULL;
    }

    aml_object_t* current = start;
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
        current = aml_object_find_child(current, segment->name);
        if (current == NULL)
        {
            LOG_ERR("unable to find intermediate AML object '%.*s'\n", AML_NAME_LENGTH, segment->name);
            return NULL;
        }
    }

    char newObjectName[AML_NAME_LENGTH + 1];
    memcpy(newObjectName, string->namePath.segments[string->namePath.segmentCount - 1].name, AML_NAME_LENGTH);
    newObjectName[AML_NAME_LENGTH] = '\0';

    return aml_object_new(current, newObjectName, flags);
}

uint64_t aml_object_init_buffer(aml_object_t* object, const uint8_t* buffer, uint64_t bytesToCopy, uint64_t length)
{
    if (object == NULL || buffer == NULL || length == 0 || bytesToCopy > length)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_BUFFER;
    object->buffer.content = heap_alloc(length, HEAP_NONE);
    if (object->buffer.content == NULL)
    {
        return ERR;
    }
    memset(object->buffer.content, 0, length);
    memcpy(object->buffer.content, buffer, bytesToCopy);
    object->buffer.length = length;

    object->buffer.byteFields = heap_alloc(sizeof(aml_object_t) * length, HEAP_NONE);
    if (object->buffer.byteFields == NULL)
    {
        heap_free(object->buffer.content);
        object->buffer.content = NULL;
        return ERR;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        object->buffer.byteFields[i] = AML_OBJECT_CREATE(AML_OBJECT_NONE);
        if (aml_object_init_buffer_field(&object->buffer.byteFields[i], object->buffer.content, i * 8, 8) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_object_deinit(&object->buffer.byteFields[j]);
            }
            heap_free(object->buffer.byteFields);
            object->buffer.byteFields = NULL;
            heap_free(object->buffer.content);
            object->buffer.content = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_object_init_buffer_empty(aml_object_t* object, uint64_t length)
{
    if (object == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_BUFFER;
    object->buffer.content = heap_alloc(length, HEAP_NONE);
    if (object->buffer.content == NULL)
    {
        return ERR;
    }
    memset(object->buffer.content, 0, length);
    object->buffer.length = length;

    object->buffer.byteFields = heap_alloc(sizeof(aml_object_t) * length, HEAP_NONE);
    if (object->buffer.byteFields == NULL)
    {
        heap_free(object->buffer.content);
        object->buffer.content = NULL;
        return ERR;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        object->buffer.byteFields[i] = AML_OBJECT_CREATE(AML_OBJECT_NONE);
        if (aml_object_init_buffer_field(&object->buffer.byteFields[i], object->buffer.content, i * 8, 8) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_object_deinit(&object->buffer.byteFields[j]);
            }
            heap_free(object->buffer.byteFields);
            object->buffer.byteFields = NULL;
            heap_free(object->buffer.content);
            object->buffer.content = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_object_init_buffer_field(aml_object_t* object, uint8_t* buffer, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || buffer == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_BUFFER_FIELD;
    object->bufferField.buffer = buffer;
    object->bufferField.bitOffset = bitOffset;
    object->bufferField.bitSize = bitSize;

    return 0;
}

uint64_t aml_object_init_device(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_DEVICE;
    memset(&object->device, 0, sizeof(object->device));

    return 0;
}

uint64_t aml_object_init_field_unit_field(aml_object_t* object, aml_object_t* opregion, aml_field_flags_t flags,
    aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || opregion == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_FIELD_UNIT;
    object->fieldUnit.type = AML_FIELD_UNIT_FIELD;
    object->fieldUnit.opregion = opregion;
    object->fieldUnit.flags = flags;
    object->fieldUnit.bitOffset = bitOffset;
    object->fieldUnit.bitSize = bitSize;
    object->fieldUnit.regionSpace = opregion->opregion.space;

    return 0;
}

uint64_t aml_object_init_field_unit_index_field(aml_object_t* object, aml_object_t* indexObject, aml_object_t* dataObject,
    aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || indexObject == NULL || dataObject == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_FIELD_UNIT;
    object->fieldUnit.type = AML_FIELD_UNIT_INDEX_FIELD;
    object->fieldUnit.indexObject = indexObject;
    object->fieldUnit.dataObject = dataObject;
    object->fieldUnit.flags = flags;
    object->fieldUnit.bitOffset = bitOffset;
    object->fieldUnit.bitSize = bitSize;
    object->fieldUnit.regionSpace = dataObject->fieldUnit.regionSpace;

    return 0;
}

uint64_t aml_object_init_field_unit_bank_field(aml_object_t* object, aml_object_t* opregion, aml_object_t* bank,
    uint64_t bankValue, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || opregion == NULL || bank == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_FIELD_UNIT;
    object->fieldUnit.type = AML_FIELD_UNIT_BANK_FIELD;
    object->fieldUnit.opregion = opregion;
    object->fieldUnit.bank = bank;
    object->fieldUnit.bankValue = bankValue;
    object->fieldUnit.flags = flags;
    object->fieldUnit.bitOffset = bitOffset;
    object->fieldUnit.bitSize = bitSize;
    object->fieldUnit.regionSpace = opregion->opregion.space;

    return 0;
}

uint64_t aml_object_init_integer(aml_object_t* object, uint64_t value)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_INTEGER;
    object->integer.value = value;

    return 0;
}

uint64_t aml_object_init_integer_constant(aml_object_t* object, uint64_t value)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_INTEGER_CONSTANT;
    object->integerConstant.value = value;

    return 0;
}

uint64_t aml_object_init_method(aml_object_t* object, aml_method_flags_t* flags, const uint8_t* start, const uint8_t* end,
    aml_method_implementation_t implementation)
{
    if (object == NULL || ((start == 0 || end == 0 || start > end) && implementation == NULL))
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_METHOD;
    object->method.implementation = implementation;
    object->method.flags = *flags;
    object->method.start = start;
    object->method.end = end;
    mutex_init(&object->method.mutex);

    return 0;
}

uint64_t aml_object_init_mutex(aml_object_t* object, aml_sync_level_t syncLevel)
{
    if (object == NULL || syncLevel > 15)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_MUTEX;
    object->mutex.syncLevel = syncLevel;
    mutex_init(&object->mutex.mutex);

    return 0;
}

uint64_t aml_object_init_object_reference(aml_object_t* object, aml_object_t* target)
{
    if (object == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_OBJECT_REFERENCE;
    object->objectReference.target = target;

    return 0;
}

uint64_t aml_object_init_operation_region(aml_object_t* object, aml_region_space_t space, uint64_t offset, uint32_t length)
{
    if (object == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_OPERATION_REGION;
    object->opregion.space = space;
    object->opregion.offset = offset;
    object->opregion.length = length;

    return 0;
}

uint64_t aml_object_init_package(aml_object_t* object, uint64_t length)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_PACKAGE;

    if (length == 0)
    {
        object->package.length = 0;
        object->package.elements = NULL;
        return 0;
    }

    object->package.length = length;
    object->package.elements = heap_alloc(sizeof(aml_object_t) * length, HEAP_NONE);
    if (object->package.elements == NULL)
    {
        return ERR;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        object->package.elements[i] = aml_object_new(NULL, "____", AML_OBJECT_NONE);
        if (object->package.elements[i] == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_object_free(object->package.elements[j]);
            }
            heap_free(object->package.elements);
            object->package.elements = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_object_init_processor(aml_object_t* object, aml_proc_id_t procId, aml_pblk_addr_t pblkAddr,
    aml_pblk_len_t pblkLen)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_PROCESSOR;
    object->processor.procId = procId;
    object->processor.pblkAddr = pblkAddr;
    object->processor.pblkLen = pblkLen;

    return 0;
}

uint64_t aml_object_init_string(aml_object_t* object, const char* str)
{
    if (object == NULL || str == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_STRING;
    uint64_t strLen = strlen(str);

    if (strLen == 0)
    {
        object->string.content = heap_alloc(1, HEAP_NONE);
        if (object->string.content == NULL)
        {
            return ERR;
        }
        object->string.content[0] = '\0';
        object->string.length = 0;
        object->string.byteFields = NULL;
        return 0;
    }

    object->string.content = heap_alloc(strLen + 1, HEAP_NONE);
    if (object->string.content == NULL)
    {
        return ERR;
    }
    object->string.length = strLen;
    memcpy(object->string.content, str, strLen);
    object->string.content[strLen] = '\0';

    object->string.byteFields = heap_alloc(sizeof(aml_object_t) * strLen, HEAP_NONE);
    if (object->string.byteFields == NULL)
    {
        heap_free(object->string.content);
        object->string.content = NULL;
        return ERR;
    }

    for (uint64_t i = 0; i < strLen; i++)
    {
        object->string.byteFields[i] = AML_OBJECT_CREATE(AML_OBJECT_NONE);
        if (aml_object_init_buffer_field(&object->string.byteFields[i], (uint8_t*)object->string.content, i * 8, 8) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_object_deinit(&object->string.byteFields[j]);
            }
            heap_free(object->string.content);
            object->string.content = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_object_init_string_empty(aml_object_t* object, uint64_t length)
{
    if (object == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_STRING;

    object->string.content = heap_alloc(length + 1, HEAP_NONE);
    if (object->string.content == NULL)
    {
        return ERR;
    }
    memset(object->string.content, '0', length);

    object->string.length = length;

    object->string.byteFields = heap_alloc(sizeof(aml_object_t) * length, HEAP_NONE);
    if (object->string.byteFields == NULL)
    {
        heap_free(object->string.content);
        object->string.content = NULL;
        return ERR;
    }

    for (uint64_t i = 0; i < length; i++)
    {
        object->string.byteFields[i] = AML_OBJECT_CREATE(AML_OBJECT_NONE);
        if (aml_object_init_buffer_field(&object->string.byteFields[i], (uint8_t*)object->string.content, i * 8, 8) == ERR)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                aml_object_deinit(&object->string.byteFields[j]);
            }
            heap_free(object->string.content);
            object->string.content = NULL;
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_object_init_unresolved(aml_object_t* object, aml_name_string_t* nameString, aml_object_t* start,
    aml_patch_up_resolve_callback_t callback)
{
    if (object == NULL || start == NULL || nameString == NULL || nameString->namePath.segmentCount == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_UNRESOLVED;
    object->unresolved.nameString = *nameString;
    object->unresolved.start = start;
    object->unresolved.callback = callback;

    if (aml_patch_up_add_unresolved(object, callback) == ERR)
    {
        object->type = AML_DATA_UNINITALIZED;
        return ERR;
    }

    return 0;
}

uint64_t aml_object_init_alias(aml_object_t* object, aml_object_t* target)
{
    if (object == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_DATA_UNINITALIZED)
    {
        aml_object_deinit(object);
    }

    object->type = AML_DATA_ALIAS;
    object->alias.target = target;

    return 0;
}

void aml_object_deinit(aml_object_t* object)
{
    if (object == NULL)
    {
        return;
    }

    switch (object->type)
    {
    case AML_DATA_UNINITALIZED:
    case AML_DATA_BUFFER_FIELD:
    case AML_DATA_DEVICE:
    case AML_DATA_INTEGER:
    case AML_DATA_INTEGER_CONSTANT:
    case AML_DATA_OBJECT_REFERENCE:
    case AML_DATA_OPERATION_REGION:
    case AML_DATA_PROCESSOR:
    case AML_DATA_RAW_DATA_BUFFER:
    case AML_DATA_DEBUG_OBJECT:
    case AML_DATA_EVENT:
    case AML_DATA_POWER_RESOURCE:
    case AML_DATA_THERMAL_ZONE:
    case AML_DATA_ALIAS:
    case AML_DATA_FIELD_UNIT:
        // Nothing to do.
        break;
    case AML_DATA_BUFFER:
        if (object->buffer.byteFields != NULL)
        {
            for (uint64_t i = 0; i < object->buffer.length; i++)
            {
                aml_object_deinit(&object->buffer.byteFields[i]);
            }
            heap_free(object->buffer.byteFields);
        }
        if (object->buffer.content != NULL)
        {
            heap_free(object->buffer.content);
        }
        object->buffer.length = 0;
        object->buffer.content = NULL;
        object->buffer.byteFields = NULL;
        break;
    case AML_DATA_MUTEX:
        mutex_deinit(&object->mutex.mutex);
        break;
    case AML_DATA_PACKAGE:
        if (object->package.elements != NULL)
        {
            for (uint64_t i = 0; i < object->package.length; i++)
            {
                aml_object_free(object->package.elements[i]);
            }
            heap_free(object->package.elements);
        }
        object->package.length = 0;
        object->package.elements = NULL;
        break;
    case AML_DATA_METHOD:
        mutex_deinit(&object->method.mutex);
        break;
    case AML_DATA_STRING:
        if (object->string.byteFields != NULL)
        {
            for (uint64_t i = 0; i < object->string.length; i++)
            {
                aml_object_deinit(&object->string.byteFields[i]);
            }
            heap_free(object->string.byteFields);
        }
        if (object->string.content != NULL)
        {
            heap_free(object->string.content);
        }
        object->string.length = 0;
        object->string.content = NULL;
        break;
    case AML_DATA_UNRESOLVED:
        aml_patch_up_remove_unresolved(object);
        break;
    default:
        panic(NULL, "unimplemented deinit of AML object '%.*s' of type '%s'\n", AML_NAME_LENGTH, object->segment,
            aml_data_type_to_string(object->type));
    }

    object->type = AML_DATA_UNINITALIZED;
}

aml_object_t* aml_object_traverse_alias(aml_object_t* object)
{
    while (object != NULL && object->type == AML_DATA_ALIAS)
    {
        object = object->alias.target;
    }
    return object;
}

aml_object_t* aml_object_find_child(aml_object_t* parent, const char* name)
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

    aml_object_t* child = NULL;
    LIST_FOR_EACH(child, &parent->children, entry)
    {
        if (aml_is_name_equal(child->segment, name))
        {
            if (child->type == AML_DATA_ALIAS)
            {
                return aml_object_traverse_alias(child);
            }
            return child;
        }
    }

    errno = ENOENT;
    return NULL;
}

aml_object_t* aml_object_find(aml_object_t* start, const char* path)
{
    if (path == NULL || path[0] == '\0')
    {
        errno = EINVAL;
        return NULL;
    }

    if (start->type == AML_DATA_ALIAS)
    {
        start = aml_object_traverse_alias(start);
    }

    const char* ptr = path;
    aml_object_t* current = start;

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

        current = aml_object_find_child(current, segment);
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
uint64_t aml_object_put_bits_at(aml_object_t* object, uint64_t value, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || bitSize == 0 || bitSize > AML_INTEGER_BIT_WIDTH)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (object->type)
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

        object->integer.value &= ~(mask << bitOffset);
        object->integer.value |= (value & mask) << bitOffset;
        break;
    case AML_DATA_BUFFER:
        if (bitOffset + bitSize > object->buffer.length * 8)
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
                object->buffer.content[bytePos] |= (1 << bitPos);
            }
            else
            {
                object->buffer.content[bytePos] &= ~(1 << bitPos);
            }
        }
        break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

uint64_t aml_object_get_bits_at(aml_object_t* object, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint64_t* out)
{
    if (object == NULL || out == NULL || bitSize == 0 || bitSize > 64)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (object->type)
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

        *out = (object->integer.value >> bitOffset) & mask;
        break;
    case AML_DATA_INTEGER_CONSTANT:
        if (bitOffset + bitSize > AML_INTEGER_BIT_WIDTH)
        {
            errno = EINVAL;
            return ERR;
        }

        if (bitSize == 64)
        {
            mask = ~UINT64_C(0);
        }
        else
        {
            mask = (UINT64_C(1) << bitSize) - 1;
        }

        *out = (object->integerConstant.value >> bitOffset) & mask;
        break;
    case AML_DATA_BUFFER:
        if (bitOffset + bitSize > object->buffer.length * 8)
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

            if (object->buffer.content[bytePos] & (1 << bitPos))
            {
                *out |= (UINT64_C(1) << i);
            }
        }
        break;
    case AML_DATA_STRING:
        if (bitOffset + bitSize > strlen(object->string.content) * 8)
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

            if (object->string.content[bytePos] & (1 << bitPos))
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
