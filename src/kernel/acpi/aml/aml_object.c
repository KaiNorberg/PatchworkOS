#include "aml_object.h"

#include "acpi/acpi.h"
#include "aml.h"
#include "aml_to_string.h"
#include "aml_token.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"

#include <errno.h>
#include <stddef.h>
#include <sys/math.h>

// Used to check for memory leaks
static uint64_t totalObjects = 0;

static bool aml_name_is_equal(const char* name1, const char* name2)
{
    if (name1 == NULL || name2 == NULL)
    {
        return false;
    }

    return memcmp(name1, name2, AML_NAME_LENGTH) == 0;
}

static uint64_t aml_object_container_get_list(aml_object_t* object, list_t** outList)
{
    if (object == NULL || outList == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    *outList = NULL;
    switch (object->type)
    {
    case AML_DEVICE:
    {
        *outList = &object->device.namedObjects;
    }
    break;
    case AML_PROCESSOR:
    {
        *outList = &object->processor.namedObjects;
    }
    break;
    case AML_METHOD:
    {
        *outList = &object->method.namedObjects;
    }
    break;
    case AML_THERMAL_ZONE:
    {
        *outList = &object->thermalZone.namedObjects;
    }
    break;
    case AML_POWER_RESOURCE:
    {
        *outList = &object->powerResource.namedObjects;
    }
    break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

static void aml_object_container_free_children(aml_object_t* object)
{
    if (object == NULL)
    {
        return;
    }

    list_t* list = NULL;
    if (aml_object_container_get_list(object, &list) == ERR)
    {
        return;
    }

    if (list == NULL)
    {
        return;
    }

    while (!list_is_empty(list))
    {
        aml_object_t* child = CONTAINER_OF(list_pop(list), aml_object_t, name.entry);
        if (child->flags & AML_OBJECT_NAMED)
        {
            child->name.parent = NULL;
            sysfs_dir_deinit(&child->name.dir);
            child->flags &= ~AML_OBJECT_NAMED;
        }
        DEREF(child);
    }

    return;
}

uint64_t aml_object_get_total_count(void)
{
    return totalObjects;
}

static void aml_object_free(aml_object_t* object)
{
    if (object == NULL)
    {
        return;
    }

    if (object->flags & AML_OBJECT_NAMED)
    {
        object->name.parent = NULL;
        sysfs_dir_deinit(&object->name.dir);
    }

    aml_object_deinit(object);

    heap_free(object);
    totalObjects--;
}

aml_object_t* aml_object_new(aml_state_t* state, aml_object_flags_t flags)
{
    aml_object_t* object = heap_alloc(sizeof(aml_object_t), HEAP_NONE);
    if (object == NULL)
    {
        return NULL;
    }
    memset(object, 0, sizeof(aml_object_t));

    ref_init(&object->ref, aml_object_free);
    list_entry_init(&object->stateEntry);
    object->flags = flags;
    object->type = AML_UNINITIALIZED;

    if (state != NULL)
    {
        list_push(&state->createdObjects, &REF(object)->stateEntry);
    }
    totalObjects++;
    return object;
}

void aml_object_deinit(aml_object_t* object)
{
    if (object == NULL)
    {
        return;
    }

    if (object->type == AML_UNINITIALIZED)
    {
        return;
    }

    switch (object->type)
    {
    case AML_BUFFER:
        if (object->buffer.content != NULL)
        {
            heap_free(object->buffer.content);
        }
        if (object->buffer.byteFields != NULL)
        {
            for (uint64_t i = 0; i < object->buffer.length; i++)
            {
                if (object->buffer.byteFields[i] != NULL)
                {
                    DEREF(object->buffer.byteFields[i]);
                }
            }
            heap_free(object->buffer.byteFields);
        }
        object->buffer.content = NULL;
        object->buffer.length = 0;
        object->buffer.byteFields = NULL;
        break;
    case AML_BUFFER_FIELD:
        if (object->bufferField.buffer != NULL)
        {
            assert(!object->bufferField.isString);
            DEREF(object->bufferField.buffer);
        }
        if (object->bufferField.string != NULL)
        {
            assert(object->bufferField.isString);
            DEREF(object->bufferField.string);
        }
        object->bufferField.isString = false;
        object->bufferField.buffer = NULL;
        object->bufferField.string = NULL;
        object->bufferField.bitOffset = 0;
        object->bufferField.bitSize = 0;
        break;
    case AML_DEVICE:
        aml_object_container_free_children(object);
        break;
    case AML_EVENT:
        // Nothing to deinitialize yet
        break;
    case AML_FIELD_UNIT:
        switch (object->fieldUnit.fieldType)
        {
        case AML_FIELD_UNIT_INDEX_FIELD:
            DEREF(object->fieldUnit.index);
            object->fieldUnit.index = NULL;
            DEREF(object->fieldUnit.data);
            object->fieldUnit.data = NULL;
            break;
        case AML_FIELD_UNIT_BANK_FIELD:
            DEREF(object->fieldUnit.bank);
            object->fieldUnit.bank = NULL;
            DEREF(object->fieldUnit.bankValue);
            object->fieldUnit.bankValue = NULL;
            DEREF(object->fieldUnit.opregion);
            object->fieldUnit.opregion = NULL;
            break;
        case AML_FIELD_UNIT_FIELD:
            DEREF(object->fieldUnit.opregion);
            object->fieldUnit.opregion = NULL;
            break;
        default:
            break;
        }
        object->fieldUnit.fieldType = 0;
        object->fieldUnit.bitOffset = 0;
        object->fieldUnit.bitSize = 0;
        break;
    case AML_INTEGER:
        object->integer.value = 0;
        break;
    case AML_INTEGER_CONSTANT:
        object->integerConstant.value = 0;
        break;
    case AML_METHOD:
        object->method.implementation = NULL;
        object->method.methodFlags = (aml_method_flags_t){0};
        object->method.start = NULL;
        object->method.end = NULL;
        aml_object_container_free_children(object);
        aml_mutex_id_deinit(&object->method.mutex);
        break;
    case AML_MUTEX:
        object->mutex.syncLevel = 0;
        aml_mutex_id_deinit(&object->mutex.mutex);
        break;
    case AML_OBJECT_REFERENCE:
        if (object->objectReference.target != NULL)
        {
            DEREF(object->objectReference.target);
        }
        object->objectReference.target = NULL;
        break;
    case AML_OPERATION_REGION:
        object->opregion.space = 0;
        object->opregion.offset = 0;
        object->opregion.length = 0;
        break;
    case AML_PACKAGE:
        if (object->package.elements != NULL)
        {
            for (uint64_t i = 0; i < object->package.length; i++)
            {
                DEREF(object->package.elements[i]);
            }
            heap_free(object->package.elements);
        }
        object->package.elements = NULL;
        object->package.length = 0;
        break;
    case AML_POWER_RESOURCE:
        object->powerResource.systemLevel = 0;
        object->powerResource.resourceOrder = 0;
        aml_object_container_free_children(object);
        break;
    case AML_PROCESSOR:
        object->processor.procId = 0;
        object->processor.pblkAddr = 0;
        object->processor.pblkLen = 0;
        aml_object_container_free_children(object);
        break;
    case AML_STRING:
        if (object->string.content != NULL)
        {
            heap_free(object->string.content);
        }
        if (object->string.byteFields != NULL)
        {
            for (uint64_t i = 0; i < object->string.length; i++)
            {
                DEREF(object->string.byteFields[i]);
            }
            heap_free(object->string.byteFields);
        }
        object->string.content = NULL;
        object->string.length = 0;
        object->string.byteFields = NULL;
        break;
    case AML_THERMAL_ZONE:
        aml_object_container_free_children(object);
        break;
    case AML_ALIAS:
        if (object->alias.target != NULL)
        {
            DEREF(object->alias.target);
        }
        object->alias.target = NULL;
        break;
    case AML_UNRESOLVED:
        aml_patch_up_remove_unresolved(&object->unresolved);
        if (object->unresolved.from != NULL)
        {
            DEREF(object->unresolved.from);
        }
        object->unresolved.from = NULL;
        object->unresolved.nameString = (aml_name_string_t){0};
        object->unresolved.callback = NULL;
        break;
    default:
        panic(NULL, "Unknown AML data type %u", object->type);
        break;
    }

    object->type = AML_UNINITIALIZED;
}

uint64_t aml_object_count_children(aml_object_t* parent)
{
    if (parent == NULL)
    {
        return 0;
    }

    uint64_t count = 0;

    if (parent->type & AML_CONTAINERS)
    {
        list_t* parentList = NULL;
        if (aml_object_container_get_list(parent, &parentList) == ERR)
        {
            return 0;
        }

        if (parentList != NULL)
        {
            aml_object_t* child = NULL;
            LIST_FOR_EACH(child, parentList, name.entry)
            {
                count++;
                count += aml_object_count_children(child);
            }
        }

        return count;
    }

    switch (parent->type)
    {
    case AML_PACKAGE:
        for (uint64_t i = 0; i < parent->package.length; i++)
        {
            aml_object_t* element = parent->package.elements[i];
            if (element != NULL)
            {
                count++;
                count += aml_object_count_children(element);
            }
        }
        break;
    case AML_BUFFER:
        if (parent->buffer.byteFields != NULL)
        {
            for (uint64_t i = 0; i < parent->buffer.length; i++)
            {
                aml_object_t* byteField = parent->buffer.byteFields[i];
                if (byteField != NULL)
                {
                    count++;
                    count += aml_object_count_children(byteField);
                }
            }
        }
        break;
    case AML_STRING:
        if (parent->string.byteFields != NULL)
        {
            for (uint64_t i = 0; i < parent->string.length; i++)
            {
                aml_object_t* byteField = parent->string.byteFields[i];
                if (byteField != NULL)
                {
                    count++;
                    count += aml_object_count_children(byteField);
                }
            }
        }
        break;
    case AML_FIELD_UNIT:
        if (parent->fieldUnit.bankValue != NULL)
        {
            count++;
        }
        break;
    default:
        break;
    }

    return count;
}

uint64_t aml_object_add_child(aml_object_t* parent, aml_object_t* child, const char* name)
{
    if (parent == NULL || child == NULL || name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (!(parent->flags & AML_OBJECT_NAMED))
    {
        LOG_ERR("Parent object is not named\n");
        errno = EINVAL;
        return ERR;
    }

    uint64_t nameLen = strnlen_s(name, AML_NAME_LENGTH);
    if (nameLen < AML_NAME_LENGTH)
    {
        errno = EILSEQ;
        return ERR;
    }

    list_entry_init(&child->name.entry);
    child->name.parent = parent;
    memcpy(child->name.segment, name, AML_NAME_LENGTH);
    child->name.segment[AML_NAME_LENGTH] = '\0';

    if (sysfs_dir_init(&child->name.dir, &parent->name.dir, child->name.segment, NULL, NULL) == ERR)
    {
        LOG_ERR("Failed to create sysfs dir for object '%s' (errno '%s')\n", AML_OBJECT_GET_NAME(child),
            strerror(errno));
        return ERR;
    }

    list_t* parentList = NULL;
    if (aml_object_container_get_list(parent, &parentList) == ERR)
    {
        sysfs_dir_deinit(&child->name.dir);
        return ERR;
    }

    aml_object_t* existing = NULL;
    LIST_FOR_EACH(existing, parentList, name.entry)
    {
        if (aml_name_is_equal(existing->name.segment, child->name.segment))
        {
            LOG_ERR("An object named '%.*s' already exists in parent '%s'\n", AML_OBJECT_GET_NAME(parent),
                AML_NAME_LENGTH, parent->name.segment);
            sysfs_dir_deinit(&child->name.dir);
            errno = EEXIST;
            return ERR;
        }
    }

    list_push(parentList, &REF(child)->name.entry);
    child->flags |= AML_OBJECT_NAMED;
    return 0;
}

uint64_t aml_object_add(aml_object_t* object, aml_object_t* from, const aml_name_string_t* nameString)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->flags & AML_OBJECT_NAMED)
    {
        LOG_ERR("Object is already named as '%s'\n", AML_OBJECT_GET_NAME(object));
        errno = EINVAL;
        return ERR;
    }

    if (object->flags & AML_OBJECT_ROOT)
    {
        if (from != NULL || nameString != NULL)
        {
            errno = EINVAL;
            return ERR;
        }

        aml_object_t* root = aml_root_get();
        if (root != NULL && root != object)
        {
            LOG_ERR("Root object already exists\n");
            errno = EEXIST;
            return ERR;
        }

        list_entry_init(&object->name.entry);
        object->name.segment[0] = '\\';
        object->name.segment[1] = '_';
        object->name.segment[2] = '_';
        object->name.segment[3] = '_';
        object->name.segment[4] = '\0';
        object->name.parent = NULL;
        if (sysfs_dir_init(&object->name.dir, acpi_get_sysfs_root(), "namespace", NULL, NULL) == ERR)
        {
            LOG_ERR("Failed to create sysfs dir for root object (errno '%s')\n", strerror(errno));
            return ERR;
        }
        object->flags |= AML_OBJECT_NAMED;
        return 0;
    }

    if (nameString == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (nameString->namePath.segmentCount == 0)
    {
        errno = EILSEQ;
        return ERR;
    }

    aml_object_t* current = from;
    if (from == NULL || nameString->rootChar.present)
    {
        current = aml_root_get();
    }

    for (uint64_t i = 0; i < nameString->prefixPath.depth; i++)
    {
        current = current->name.parent;
        if (current == NULL)
        {
            errno = ENOENT;
            return ERR;
        }
    }

    for (uint8_t i = 0; i < nameString->namePath.segmentCount - 1; i++)
    {
        const aml_name_seg_t* segment = &nameString->namePath.segments[i];
        current = aml_object_find_child(current, segment->name);
        if (current == NULL)
        {
            LOG_ERR("unable to find intermediate AML object '%s' in path '%s'\n", segment->name,
                aml_name_string_to_string(nameString));
            return ERR;
        }
    }
    aml_object_t* parent = current;

    char segmentName[AML_NAME_LENGTH + 1];
    aml_name_seg_t* segment = &nameString->namePath.segments[nameString->namePath.segmentCount - 1];
    memcpy(segmentName, segment->name, AML_NAME_LENGTH);
    segmentName[AML_NAME_LENGTH] = '\0';

    return aml_object_add_child(parent, object, segmentName);
}

uint64_t aml_object_remove(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (!(object->flags & AML_OBJECT_NAMED))
    {
        return 0;
    }

    if (object->name.parent == NULL)
    {
        LOG_ERR("Cannot remove root object\n");
        errno = EINVAL;
        return ERR;
    }

    list_t* parentList = NULL;
    if (aml_object_container_get_list(object->name.parent, &parentList) == ERR)
    {
        return ERR;
    }

    list_remove(parentList, &object->name.entry);
    object->name.parent = NULL;
    sysfs_dir_deinit(&object->name.dir);
    object->flags &= ~AML_OBJECT_NAMED;
    DEREF(object);
    return 0;
}

aml_object_t* aml_object_find_child(aml_object_t* parent, const char* name)
{
    if (parent == NULL || name == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    list_t* parentList = NULL;
    if (aml_object_container_get_list(parent, &parentList) == ERR)
    {
        return NULL;
    }

    aml_object_t* child = NULL;
    LIST_FOR_EACH(child, parentList, name.entry)
    {
        if (aml_name_is_equal(child->name.segment, name))
        {
            if (child->type == AML_ALIAS)
            {
                aml_alias_t* alias = &child->alias;
                if (alias->target == NULL)
                {
                    LOG_ERR("Alias object '%s' has no target\n", AML_OBJECT_GET_NAME(child));
                    errno = ENOENT;
                    return NULL;
                }

                aml_object_t* target = aml_alias_traverse(alias);
                if (target == NULL)
                {
                    LOG_ERR("Failed to traverse alias object '%s'\n", AML_OBJECT_GET_NAME(child));
                    return NULL;
                }

                return target;
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

    if (start != NULL && !(start->flags & AML_OBJECT_NAMED))
    {
        errno = EINVAL;
        return NULL;
    }

    const char* p = path;
    if (p[0] == '\\')
    {
        start = aml_root_get();
        p++;
    }
    else if (p[0] == '^')
    {
        if (start == NULL)
        {
            errno = EINVAL;
            return NULL;
        }

        while (p[0] == '^')
        {
            start = start->name.parent;
            if (start == NULL)
            {
                errno = ENOENT;
                return NULL;
            }
            p++;
        }
    }
    else if (start == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    aml_object_t* current = start;
    while (*p != '\0')
    {
        const char* segmentStart = p;
        while (*p != '.' && *p != '\0')
        {
            p++;
        }
        uint64_t segmentLength = p - segmentStart;

        if (segmentLength > AML_NAME_LENGTH)
        {
            errno = EILSEQ;
            return NULL;
        }

        char segment[AML_NAME_LENGTH + 1];
        memcpy(segment, segmentStart, segmentLength);
        segment[segmentLength] = '\0';

        if (*p == '.')
        {
            p++;
        }

        if (!(current->type & AML_CONTAINERS))
        {
            if (start->name.parent != NULL)
            {
                return aml_object_find(start->name.parent, path);
            }
            errno = ENOENT;
            return NULL;
        }

        current = aml_object_find_child(current, segment);
        if (current == NULL)
        {
            if (start->name.parent != NULL)
            {
                return aml_object_find(start->name.parent, path);
            }
            return NULL;
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
    case AML_INTEGER:
    {
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
    }
    break;
    case AML_BUFFER:
    {
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
    case AML_INTEGER:
    {
        if (bitOffset + bitSize > AML_INTEGER_BIT_WIDTH)
        {
            *out = 0;
            return 0;
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
    }
    break;
    case AML_INTEGER_CONSTANT:
    {
        if (bitOffset + bitSize > AML_INTEGER_BIT_WIDTH)
        {
            *out = 0;
            return 0;
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

        *out = (object->integerConstant.value >> bitOffset) & mask;
    }
    break;
    case AML_BUFFER:
    {
        if (bitOffset + bitSize > object->buffer.length * 8)
        {
            *out = 0;
            return 0;
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
    }
    break;
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

static inline uint64_t aml_object_check_deinit(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_UNINITIALIZED)
    {
        aml_object_deinit(object);
    }

    return 0;
}

uint64_t aml_buffer_init_empty(aml_object_t* object, uint64_t length)
{
    if (object == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->buffer.content = heap_alloc(length, HEAP_NONE);
    if (object->buffer.content == NULL)
    {
        return ERR;
    }
    memset(object->buffer.content, 0, length);
    object->buffer.length = length;
    object->buffer.byteFields = NULL;
    object->type = AML_BUFFER;
    return 0;
}

uint64_t aml_buffer_init(aml_object_t* object, const uint8_t* buffer, uint64_t bytesToCopy, uint64_t length)
{
    if (object == NULL || buffer == NULL || length == 0 || bytesToCopy > length)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_buffer_init_empty(object, length) == ERR)
    {
        return ERR;
    }

    memcpy(object->buffer.content, buffer, bytesToCopy);
    return 0;
}

uint64_t aml_buffer_field_init_buffer(aml_object_t* object, aml_buffer_t* buffer, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize)
{
    if (object == NULL || buffer == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->bufferField.isString = false;
    object->bufferField.buffer = REF(buffer);
    object->bufferField.string = NULL;
    object->bufferField.bitOffset = bitOffset;
    object->bufferField.bitSize = bitSize;
    object->type = AML_BUFFER_FIELD;
    return 0;
}

uint64_t aml_buffer_field_init_string(aml_object_t* object, aml_string_t* string, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize)
{
    if (object == NULL || string == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->bufferField.isString = true;
    object->bufferField.buffer = NULL;
    object->bufferField.string = REF(string);
    object->bufferField.bitOffset = bitOffset;
    object->bufferField.bitSize = bitSize;
    object->type = AML_BUFFER_FIELD;
    return 0;
}

uint64_t aml_device_init(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    list_init(&object->device.namedObjects);
    object->type = AML_DEVICE;
    return 0;
}

uint64_t aml_event_init(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->type = AML_EVENT;
    return 0;
}

uint64_t aml_field_unit_field_init(aml_object_t* object, aml_opregion_t* opregion, aml_field_flags_t flags,
    aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || opregion == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->fieldUnit.fieldType = AML_FIELD_UNIT_FIELD;
    object->fieldUnit.index = NULL;
    object->fieldUnit.data = NULL;
    object->fieldUnit.bankValue = NULL;
    object->fieldUnit.bank = NULL;
    object->fieldUnit.opregion = REF(opregion);
    object->fieldUnit.fieldFlags = flags;
    object->fieldUnit.bitOffset = bitOffset;
    object->fieldUnit.bitSize = bitSize;
    object->type = AML_FIELD_UNIT;
    return 0;
}

uint64_t aml_field_unit_index_field_init(aml_object_t* object, aml_field_unit_t* index, aml_field_unit_t* data,
    aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || index == NULL || data == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->fieldUnit.fieldType = AML_FIELD_UNIT_INDEX_FIELD;
    object->fieldUnit.index = REF(index);
    object->fieldUnit.data = REF(data);
    object->fieldUnit.bankValue = NULL;
    object->fieldUnit.bank = NULL;
    object->fieldUnit.opregion = NULL;
    object->fieldUnit.fieldFlags = flags;
    object->fieldUnit.bitOffset = bitOffset;
    object->fieldUnit.bitSize = bitSize;
    object->type = AML_FIELD_UNIT;
    return 0;
}

uint64_t aml_field_unit_bank_field_init(aml_object_t* object, aml_opregion_t* opregion, aml_field_unit_t* bank,
    uint64_t bankValue, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || opregion == NULL || bank == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->fieldUnit.fieldType = AML_FIELD_UNIT_BANK_FIELD;
    object->fieldUnit.index = NULL;
    object->fieldUnit.data = NULL;

    aml_object_t* bankValueObj = aml_object_new(NULL, AML_OBJECT_NONE);
    if (bankValueObj == NULL)
    {
        return ERR;
    }
    if (aml_integer_init(bankValueObj, bankValue) == ERR)
    {
        DEREF(bankValueObj);
        return ERR;
    }
    object->fieldUnit.bankValue = bankValueObj;

    object->fieldUnit.bank = REF(bank);
    object->fieldUnit.opregion = REF(opregion);
    object->fieldUnit.fieldFlags = flags;
    object->fieldUnit.bitOffset = bitOffset;
    object->fieldUnit.bitSize = bitSize;
    object->type = AML_FIELD_UNIT;
    return 0;
}

uint64_t aml_integer_init(aml_object_t* object, uint64_t value)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->integer.value = value;
    object->type = AML_INTEGER;
    return 0;
}

uint64_t aml_integer_constant_init(aml_object_t* object, uint64_t value)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->integerConstant.value = value;
    object->type = AML_INTEGER_CONSTANT;
    return 0;
}

uint64_t aml_method_init(aml_object_t* object, aml_method_flags_t flags, const uint8_t* start, const uint8_t* end,
    aml_method_implementation_t implementation)
{
    if (object == NULL || ((start == 0 || end == 0 || start > end) && implementation == NULL))
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->method.implementation = implementation;
    object->method.methodFlags = flags;
    object->method.start = start;
    object->method.end = end;
    list_init(&object->method.namedObjects);
    aml_mutex_id_init(&object->method.mutex);
    object->type = AML_METHOD;
    return 0;
}

uint64_t aml_mutex_init(aml_object_t* object, aml_sync_level_t syncLevel)
{
    if (object == NULL || syncLevel > 15)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->mutex.syncLevel = syncLevel;
    aml_mutex_id_init(&object->mutex.mutex);
    object->type = AML_MUTEX;
    return 0;
}

uint64_t aml_object_reference_init(aml_object_t* object, aml_object_t* target)
{
    if (object == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->objectReference.target = REF(target);
    object->type = AML_OBJECT_REFERENCE;
    return 0;
}

uint64_t aml_operation_region_init(aml_object_t* object, aml_region_space_t space, uint64_t offset, uint32_t length)
{
    if (object == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->opregion.space = space;
    object->opregion.offset = offset;
    object->opregion.length = length;
    object->type = AML_OPERATION_REGION;
    return 0;
}

uint64_t aml_package_init(aml_object_t* object, uint64_t length)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->package.elements = heap_alloc(sizeof(aml_object_t*) * length, HEAP_NONE);
    if (object->package.elements == NULL)
    {
        return ERR;
    }
    for (uint64_t i = 0; i < length; i++)
    {
        object->package.elements[i] = aml_object_new(NULL, AML_OBJECT_ELEMENT);
        if (object->package.elements[i] == NULL)
        {
            for (uint64_t j = 0; j < i; j++)
            {
                DEREF(object->package.elements[j]);
            }
            heap_free(object->package.elements);
            object->package.elements = NULL;
            return ERR;
        }
    }
    object->package.length = length;
    object->type = AML_PACKAGE;
    return 0;
}

uint64_t aml_power_resource_init(aml_object_t* object, aml_system_level_t systemLevel,
    aml_resource_order_t resourceOrder)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->powerResource.systemLevel = systemLevel;
    object->powerResource.resourceOrder = resourceOrder;
    list_init(&object->powerResource.namedObjects);
    object->type = AML_POWER_RESOURCE;
    return 0;
}

uint64_t aml_processor_init(aml_object_t* object, aml_proc_id_t procId, aml_pblk_addr_t pblkAddr,
    aml_pblk_len_t pblkLen)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->processor.procId = procId;
    object->processor.pblkAddr = pblkAddr;
    object->processor.pblkLen = pblkLen;
    list_init(&object->processor.namedObjects);
    object->type = AML_PROCESSOR;
    return 0;
}

uint64_t aml_string_init_empty(aml_object_t* object, uint64_t length)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->string.content = heap_alloc(length + 1, HEAP_NONE);
    if (object->string.content == NULL)
    {
        return ERR;
    }
    memset(object->string.content, 0, length + 1);
    object->string.length = length;
    object->string.byteFields = NULL;
    object->type = AML_STRING;
    return 0;
}

uint64_t aml_string_init(aml_object_t* object, const char* str)
{
    if (object == NULL || str == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    size_t length = strlen(str);
    if (aml_string_init_empty(object, length) == ERR)
    {
        return ERR;
    }
    memcpy(object->string.content, str, length);
    return 0;
}

uint64_t aml_thermal_zone_init(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    list_init(&object->thermalZone.namedObjects);
    object->type = AML_THERMAL_ZONE;
    return 0;
}

uint64_t aml_alias_init(aml_object_t* object, aml_object_t* target)
{
    if (object == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->alias.target = REF(target);
    object->type = AML_ALIAS;
    return 0;
}

aml_object_t* aml_alias_traverse(aml_alias_t* alias)
{
    if (alias == NULL)
    {
        return NULL;
    }

    aml_object_t* current = CONTAINER_OF(alias, aml_object_t, alias);
    while (current != NULL && current->type == AML_ALIAS)
    {
        if (current->alias.target == NULL)
        {
            return NULL;
        }
        current = current->alias.target;
    }
    return current;
}

uint64_t aml_unresolved_init(aml_object_t* object, const aml_name_string_t* nameString, aml_object_t* from,
    aml_patch_up_resolve_callback_t callback)
{
    if (object == NULL || nameString == NULL || callback == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_deinit(object) == ERR)
    {
        return ERR;
    }

    object->unresolved.nameString = *nameString;
    object->unresolved.from = REF(from);
    object->unresolved.callback = callback;
    object->type = AML_UNRESOLVED;

    if (aml_patch_up_add_unresolved(&object->unresolved) == ERR)
    {
        object->type = AML_UNINITIALIZED;
        return ERR;
    }
    return 0;
}
