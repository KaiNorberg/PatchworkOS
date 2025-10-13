#include "object.h"

#include "acpi/acpi.h"
#include "aml.h"
#include "exception.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "to_string.h"
#include "token.h"

#include <errno.h>
#include <stddef.h>
#include <sys/math.h>

// Used to check for memory leaks
static uint64_t totalObjects = 0;

// Cache for aml_object_t to avoid frequent allocations
static list_t objectsCache = LIST_CREATE(objectsCache);

// Used to find the children of containers using their id and the name of the child.
static map_t objectMap;

// Used to assign unique ids to containers
static uint64_t newContainerId = 0;

static inline map_key_t aml_object_map_key(aml_container_id_t containerId, const char* name)
{
    assert(strnlen_s(name, AML_NAME_LENGTH) == AML_NAME_LENGTH);

    // Pack the containerId and name into a single buffer for the map key
    uint8_t buffer[AML_NAME_LENGTH + sizeof(aml_container_id_t)] = {containerId & 0xFF, (containerId >> 8) & 0xFF,
        (containerId >> 16) & 0xFF, (containerId >> 24) & 0xFF, (containerId >> 32) & 0xFF, (containerId >> 40) & 0xFF,
        (containerId >> 48) & 0xFF, (containerId >> 56) & 0xFF, name[0], name[1], name[2], name[3]};
    return map_key_buffer(buffer, sizeof(buffer));
}

static bool aml_name_is_equal(const char* name1, const char* name2)
{
    if (name1 == NULL || name2 == NULL)
    {
        return false;
    }

    return memcmp(name1, name2, AML_NAME_LENGTH) == 0;
}

static void aml_container_init(aml_container_t* container)
{
    if (container == NULL)
    {
        return;
    }

    list_init(&container->namedObjects);
    container->id = newContainerId++;
}

static void aml_container_deinit(aml_container_t* container)
{
    if (container == NULL)
    {
        return;
    }

    while (!list_is_empty(&container->namedObjects))
    {
        aml_object_remove(CONTAINER_OF(list_first(&container->namedObjects), aml_object_t, name.parentEntry));
    }
}

static aml_container_t* aml_object_container_get(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    switch (object->type)
    {
    case AML_DEVICE:
        return &object->device.container;
    case AML_PROCESSOR:
        return &object->processor.container;
    case AML_METHOD:
        return &object->method.container;
    case AML_THERMAL_ZONE:
        return &object->thermalZone.container;
    case AML_POWER_RESOURCE:
        return &object->powerResource.container;
    case AML_PREDEFINED_SCOPE:
        return &object->predefinedScope.container;
    default:
        errno = EINVAL;
        return NULL;
    }
}

uint64_t aml_object_get_total_count(void)
{
    return totalObjects;
}

uint64_t aml_object_map_init(void)
{
    if (map_init(&objectMap) == ERR)
    {
        return ERR;
    }
    return 0;
}

static void aml_object_free(aml_object_t* object)
{
    if (object == NULL)
    {
        return;
    }

    if (object->flags & AML_OBJECT_NAMED)
    {
        aml_object_remove(object);
    }

    aml_object_clear(object);

    if (list_length(&objectsCache) < AML_OBJECT_CACHE_SIZE)
    {
        list_push(&objectsCache, &object->cacheEntry);
    }
    else
    {
        heap_free(object);
    }

    totalObjects--;
}

aml_object_t* aml_object_new(void)
{
    aml_object_t* object = NULL;
    if (!list_is_empty(&objectsCache))
    {
        object = CONTAINER_OF_SAFE(list_pop(&objectsCache), aml_object_t, cacheEntry);
    }

    if (object == NULL)
    {
        object = heap_alloc(sizeof(aml_object_t), HEAP_NONE);
        if (object == NULL)
        {
            return NULL;
        }
        memset(object, 0, sizeof(aml_object_t));
    }

    ref_init(&object->ref, aml_object_free);
    list_entry_init(&object->cacheEntry);
    object->flags = AML_OBJECT_NONE;
    object->type = AML_UNINITIALIZED;

    strncpy(object->name.segment, AML_UNNAMED_NAME, AML_NAME_LENGTH);
    object->name.segment[AML_NAME_LENGTH] = '\0';

    totalObjects++;
    return object;
}

void aml_object_clear(aml_object_t* object)
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
        if (object->buffer.content != NULL && object->buffer.length > AML_SMALL_BUFFER_SIZE)
        {
            heap_free(object->buffer.content);
        }
        object->buffer.content = NULL;
        object->buffer.length = 0;
        break;
    case AML_BUFFER_FIELD:
        if (object->bufferField.target != NULL)
        {
            DEREF(object->bufferField.target);
        }
        object->bufferField.target = NULL;
        object->bufferField.bitOffset = 0;
        object->bufferField.bitSize = 0;
        break;
    case AML_DEBUG_OBJECT:
        break;
    case AML_DEVICE:
        aml_container_deinit(&object->device.container);
        break;
    case AML_EVENT:
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
    case AML_METHOD:
        object->method.implementation = NULL;
        object->method.methodFlags = (aml_method_flags_t){0};
        object->method.start = NULL;
        object->method.end = NULL;
        aml_container_deinit(&object->method.container);
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
                object->package.elements[i] = NULL;
            }
            if (object->package.length > AML_SMALL_PACKAGE_SIZE)
            {
                heap_free(object->package.elements);
            }
        }
        object->package.elements = NULL;
        object->package.length = 0;
        break;
    case AML_POWER_RESOURCE:
        object->powerResource.systemLevel = 0;
        object->powerResource.resourceOrder = 0;
        aml_container_deinit(&object->powerResource.container);
        break;
    case AML_PROCESSOR:
        object->processor.procId = 0;
        object->processor.pblkAddr = 0;
        object->processor.pblkLen = 0;
        aml_container_deinit(&object->processor.container);
        break;
    case AML_STRING:
        if (object->string.content != NULL && object->string.length > AML_SMALL_STRING_SIZE)
        {
            heap_free(object->string.content);
        }
        object->string.content = NULL;
        object->string.length = 0;
        break;
    case AML_THERMAL_ZONE:
        aml_container_deinit(&object->thermalZone.container);
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
    case AML_PREDEFINED_SCOPE:
        aml_container_deinit(&object->predefinedScope.container);
        break;
    case AML_LOCAL:
        if (object->local.value != NULL)
        {
            DEREF(object->local.value);
        }
        object->local.value = NULL;
        break;
    case AML_ARG:
        if (object->arg.value != NULL)
        {
            DEREF(object->arg.value);
        }
        object->arg.value = NULL;
        break;
    default:
        panic(NULL, "Unknown AML data type %u", object->type);
        break;
    }

    object->type = AML_UNINITIALIZED;
}

uint64_t aml_object_expose_in_sysfs(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (!(object->flags & AML_OBJECT_NAMED))
    {
        LOG_ERR("Object is not named\n");
        errno = EINVAL;
        return ERR;
    }

    if (!(object->flags & AML_OBJECT_EXPOSED_IN_SYSFS))
    {
        aml_container_t* container = aml_object_container_get(object->name.parent);
        if (container == NULL)
        {
            LOG_ERR("Parent object of type '%s' cannot contain children\n", aml_type_to_string(object->type));
            errno = EINVAL;
            return ERR;
        }

        if (sysfs_dir_init(&object->name.dir, &object->name.parent->name.dir, object->name.segment, NULL, NULL) == ERR)
        {
            LOG_ERR("Failed to create sysfs dir for object '%s' (errno '%s')\n", AML_OBJECT_GET_NAME(object),
                strerror(errno));
            return ERR;
        }

        object->flags |= AML_OBJECT_EXPOSED_IN_SYSFS;
    }

    if (!(object->type & AML_CONTAINERS))
    {
        return 0;
    }

    aml_container_t* container = aml_object_container_get(object);
    if (container == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_object_t* child = NULL;
    LIST_FOR_EACH(child, &container->namedObjects, name.parentEntry)
    {
        if (aml_object_expose_in_sysfs(child) == ERR)
        {
            return ERR;
        }
    }

    return 0;
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
        aml_container_t* container = aml_object_container_get(parent);
        if (container != NULL)
        {
            aml_object_t* child = NULL;
            LIST_FOR_EACH(child, &container->namedObjects, name.parentEntry)
            {
                count++;
                count += aml_object_count_children(child);
            }

            return count;
        }
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

uint64_t aml_object_add_child(aml_object_t* parent, aml_object_t* child, const char* name, aml_state_t* state)
{
    if (parent == NULL || child == NULL || name == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (child->type == AML_UNINITIALIZED)
    {
        LOG_ERR("Child object is uninitialized\n");
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

    map_entry_init(&child->name.mapEntry);
    list_entry_init(&child->name.parentEntry);
    list_entry_init(&child->name.stateEntry);
    child->name.state = state;
    child->name.parent = parent;
    memcpy(child->name.segment, name, AML_NAME_LENGTH);
    child->name.segment[AML_NAME_LENGTH] = '\0';
    sysfs_dir_deinit(&child->name.dir); // Just in case

    aml_container_t* container = aml_object_container_get(parent);
    if (container == NULL)
    {
        LOG_ERR("Parent object of type '%s' cannot contain children\n", aml_type_to_string(parent->type));
        errno = EINVAL;
        return ERR;
    }

    map_key_t key = aml_object_map_key(container->id, child->name.segment);
    if (map_insert(&objectMap, &key, &child->name.mapEntry) == ERR) // Map does not get a reference
    {
        LOG_ERR("Failed to insert object '%s' into object map (errno '%s')\n", AML_OBJECT_GET_NAME(child),
            strerror(errno));
        return ERR;
    }
    list_push(&container->namedObjects, &REF(child)->name.parentEntry); // Parent gets a reference
    if (state != NULL)
    {
        // The state does not take a reference to the object, it just keeps track of it for garbage collection.
        list_push(&state->namedObjects, &child->name.stateEntry);
    }

    child->flags |= AML_OBJECT_NAMED;
    return 0;
}

uint64_t aml_object_add(aml_object_t* object, aml_object_t* from, const aml_name_string_t* nameString,
    aml_state_t* state)
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
        DEREF_DEFER(root);
        if (root != NULL && root != object)
        {
            LOG_ERR("Root object already exists\n");
            errno = EEXIST;
            return ERR;
        }

        map_entry_init(&object->name.mapEntry);
        list_entry_init(&object->name.parentEntry);
        list_entry_init(&object->name.stateEntry);
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
        object->flags |= AML_OBJECT_NAMED | AML_OBJECT_EXPOSED_IN_SYSFS;
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

    aml_object_t* current = NULL;
    if (from == NULL || nameString->rootChar.present)
    {
        current = aml_root_get();
    }
    else
    {
        current = REF(from);
    }

    for (uint64_t i = 0; i < nameString->prefixPath.depth; i++)
    {
        aml_object_t* next = current->name.parent;
        if (next == NULL)
        {
            DEREF(current);
            errno = ENOENT;
            return ERR;
        }
        DEREF(current);
        current = next;
    }

    for (uint8_t i = 0; i < nameString->namePath.segmentCount - 1; i++)
    {
        const aml_name_seg_t* segment = &nameString->namePath.segments[i];
        aml_object_t* next = aml_object_find_child(current, segment->name);
        if (next == NULL)
        {
            DEREF(current);
            LOG_ERR("unable to find intermediate AML object '%s' in path '%s'\n", segment->name,
                aml_name_string_to_string(nameString));
            return ERR;
        }
        DEREF(current);
        current = next;
    }
    aml_object_t* parent = current;
    DEREF_DEFER(parent);

    char segmentName[AML_NAME_LENGTH + 1];
    aml_name_seg_t* segment = &nameString->namePath.segments[nameString->namePath.segmentCount - 1];
    memcpy(segmentName, segment->name, AML_NAME_LENGTH);
    segmentName[AML_NAME_LENGTH] = '\0';

    return aml_object_add_child(parent, object, segmentName, state);
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

    aml_container_t* container = aml_object_container_get(object->name.parent);
    if (container == NULL)
    {
        LOG_ERR("Parent object of type '%s' cannot contain children\n", aml_type_to_string(object->name.parent->type));
        errno = EINVAL;
        return ERR;
    }

    map_key_t key = aml_object_map_key(container->id, object->name.segment);
    map_remove(&objectMap, &key); // Map does not hold a reference

    if (object->name.state != NULL)
    {
        list_remove(&object->name.state->namedObjects, &object->name.stateEntry);
        object->name.state = NULL;
    }

    if (object->flags & AML_OBJECT_EXPOSED_IN_SYSFS)
    {
        sysfs_dir_deinit(&object->name.dir);
        object->flags &= ~AML_OBJECT_EXPOSED_IN_SYSFS;
    }

    object->flags &= ~AML_OBJECT_NAMED;

    list_remove(&container->namedObjects, &object->name.parentEntry);
    object->name.parent = NULL;
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

    aml_container_t* container = aml_object_container_get(parent);
    if (container == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    map_key_t key = aml_object_map_key(container->id, name);
    map_entry_t* entry = map_get(&objectMap, &key);
    if (entry == NULL)
    {
        errno = ENOENT;
        return NULL;
    }

    aml_object_t* child = CONTAINER_OF(entry, aml_object_t, name.mapEntry);
    if (child->type == AML_ALIAS)
    {
        aml_alias_obj_t* alias = &child->alias;
        if (alias->target == NULL)
        {
            LOG_ERR("Alias object '%s' has no target\n", AML_OBJECT_GET_NAME(child));
            errno = ENOENT;
            return NULL;
        }

        aml_object_t* target = aml_alias_obj_traverse(alias);
        if (target == NULL)
        {
            LOG_ERR("Failed to traverse alias object '%s'\n", AML_OBJECT_GET_NAME(child));
            return NULL;
        }

        return target; // Transfer ownership
    }

    return REF(child);
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

    aml_object_t* current = NULL;
    const char* p = path;
    if (p[0] == '\\')
    {
        current = aml_root_get();
        p++;
    }
    else if (p[0] == '^')
    {
        if (start == NULL)
        {
            errno = EINVAL;
            return NULL;
        }

        current = REF(start);
        while (p[0] == '^')
        {
            aml_object_t* next = current->name.parent;
            if (next == NULL)
            {
                DEREF(current);
                errno = ENOENT;
                return NULL;
            }
            DEREF(current);
            current = next;
            p++;
        }
    }
    else if (start == NULL)
    {
        errno = EINVAL;
        return NULL;
    }
    else
    {
        current = REF(start);
    }

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
            DEREF(current);
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
            DEREF(current);
            errno = ENOENT;
            return NULL;
        }

        aml_object_t* next = aml_object_find_child(current, segment);
        if (next == NULL)
        {
            aml_object_t* scope = current->name.parent;
            while (scope != NULL)
            {
                aml_object_t* found = aml_object_find_child(scope, segment);
                if (found != NULL)
                {
                    next = found;
                    break;
                }
                scope = scope->name.parent;
            }
            if (next == NULL)
            {
                DEREF(current);
                errno = ENOENT;
                return NULL;
            }
        }

        DEREF(current);
        current = next;
    }

    return current; // Transfer ownership
}

static inline void aml_copy_bits(uint8_t* dst, uint64_t dstOffset, const uint8_t* src, uint64_t srcOffset,
    uint64_t bitCount)
{
    while (bitCount > 0)
    {
        uint64_t dstByte = dstOffset / 8;
        uint64_t dstBit = dstOffset % 8;
        uint64_t srcByte = srcOffset / 8;
        uint64_t srcBit = srcOffset % 8;

        uint64_t bitsInDstByte = 8 - dstBit;
        uint64_t bitsInSrcByte = 8 - srcBit;
        uint64_t bitsToCopy = (bitsInDstByte < bitsInSrcByte) ? bitsInDstByte : bitsInSrcByte;
        bitsToCopy = (bitsToCopy < bitCount) ? bitsToCopy : bitCount;

        uint8_t srcMask = ((1 << bitsToCopy) - 1) << srcBit;
        uint8_t bits = (src[srcByte] & srcMask) >> srcBit;

        uint8_t dstMask = ((1 << bitsToCopy) - 1) << dstBit;
        dst[dstByte] = (dst[dstByte] & ~dstMask) | (bits << dstBit);

        dstOffset += bitsToCopy;
        srcOffset += bitsToCopy;
        bitCount -= bitsToCopy;
    }
}

uint64_t aml_object_set_bits_at(aml_object_t* object, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint8_t* data)
{
    if (object == NULL || data == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    switch (object->type)
    {
    case AML_INTEGER:
    {
        uint64_t value = 0;
        for (uint64_t i = 0; i < (bitSize + 7) / 8; i++)
        {
            value |= ((uint64_t)data[i]) << (i * 8);
        }

        uint64_t effectiveBitSize = bitSize;
        if (bitOffset >= aml_integer_bit_size())
        {
            return 0;
        }
        if (bitOffset + bitSize > aml_integer_bit_size())
        {
            effectiveBitSize = aml_integer_bit_size() - bitOffset;
        }

        uint64_t mask = (effectiveBitSize >= aml_integer_bit_size()) ? aml_integer_ones()
                                                                     : ((1ULL << effectiveBitSize) - 1) << bitOffset;

        object->integer.value = (object->integer.value & ~mask) | ((value << bitOffset) & mask);
        break;
    }
    case AML_BUFFER:
    case AML_STRING:
    {
        uint64_t length = object->type == AML_BUFFER ? object->buffer.length : object->string.length;
        uint8_t* content = object->type == AML_BUFFER ? object->buffer.content : (uint8_t*)object->string.content;
        uint64_t totalBits = length * 8;

        if (bitOffset >= totalBits)
        {
            return 0;
        }
        if (bitOffset + bitSize > totalBits)
        {
            bitSize = totalBits - bitOffset;
        }

        aml_copy_bits(content, bitOffset, data, 0, bitSize);
        break;
    }
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

uint64_t aml_object_get_bits_at(aml_object_t* object, aml_bit_size_t bitOffset, aml_bit_size_t bitSize, uint8_t* out)
{
    if (object == NULL || out == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    memset(out, 0, (bitSize + 7) / 8);

    switch (object->type)
    {
    case AML_INTEGER:
    {
        if (bitOffset >= aml_integer_bit_size())
        {
            return 0;
        }

        uint64_t effectiveBitSize = bitSize;
        if (bitOffset + bitSize > aml_integer_bit_size())
        {
            effectiveBitSize = aml_integer_bit_size() - bitOffset;
        }

        uint64_t mask =
            (effectiveBitSize >= aml_integer_bit_size()) ? aml_integer_ones() : ((1ULL << effectiveBitSize) - 1);
        uint64_t value = (object->integer.value >> bitOffset) & mask;

        for (uint64_t i = 0; i < (effectiveBitSize + 7) / 8; i++)
        {
            out[i] = (value >> (i * 8)) & 0xFF;
        }
        break;
    }
    case AML_BUFFER:
    case AML_STRING:
    {
        uint64_t length = object->type == AML_BUFFER ? object->buffer.length : object->string.length;
        uint8_t* content = object->type == AML_BUFFER ? object->buffer.content : (uint8_t*)object->string.content;
        uint64_t totalBits = length * 8;

        if (bitOffset >= totalBits)
        {
            return 0;
        }
        if (bitOffset + bitSize > totalBits)
        {
            bitSize = totalBits - bitOffset;
        }

        aml_copy_bits(out, 0, content, bitOffset, bitSize);
        break;
    }
    default:
        errno = EINVAL;
        return ERR;
    }

    return 0;
}

void aml_object_exception_check(aml_object_t* object)
{
    if (object->flags & AML_OBJECT_EXCEPTION_ON_USE)
    {
        AML_EXCEPTION_RAISE(AML_PARSE); // Not fatal.
        object->flags &= ~AML_OBJECT_EXCEPTION_ON_USE;
        // We can still use the object, so continue.
    }
}

static inline uint64_t aml_object_check_clear(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type != AML_UNINITIALIZED)
    {
        aml_object_clear(object);
    }

    return 0;
}

uint64_t aml_buffer_set_empty(aml_object_t* object, uint64_t length)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    if (length <= AML_SMALL_BUFFER_SIZE)
    {
        object->buffer.content = object->buffer.smallBuffer;
        memset(object->buffer.content, 0, length);
        object->buffer.length = length;
        object->type = AML_BUFFER;
        return 0;
    }

    object->buffer.content = heap_alloc(length, HEAP_NONE);
    if (object->buffer.content == NULL)
    {
        return ERR;
    }
    memset(object->buffer.content, 0, length);
    object->buffer.length = length;
    object->type = AML_BUFFER;
    return 0;
}

uint64_t aml_buffer_set(aml_object_t* object, const uint8_t* buffer, uint64_t bytesToCopy, uint64_t length)
{
    if (object == NULL || buffer == NULL || bytesToCopy > length)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_buffer_set_empty(object, length) == ERR)
    {
        return ERR;
    }

    memcpy(object->buffer.content, buffer, bytesToCopy);
    return 0;
}

uint64_t aml_buffer_field_set(aml_object_t* object, aml_object_t* target, aml_bit_size_t bitOffset,
    aml_bit_size_t bitSize)
{
    if (object == NULL || target == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (target->type != AML_BUFFER && target->type != AML_STRING)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->bufferField.target = REF(target);
    object->bufferField.bitOffset = bitOffset;
    object->bufferField.bitSize = bitSize;
    object->type = AML_BUFFER_FIELD;
    return 0;
}

uint64_t aml_debug_object_set(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->type = AML_DEBUG_OBJECT;
    return 0;
}

uint64_t aml_device_set(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    aml_container_init(&object->device.container);
    object->type = AML_DEVICE;
    return 0;
}

uint64_t aml_event_set(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->type = AML_EVENT;
    return 0;
}

uint64_t aml_field_unit_field_set(aml_object_t* object, aml_opregion_obj_t* opregion, aml_field_flags_t flags,
    aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || opregion == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
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

uint64_t aml_field_unit_index_field_set(aml_object_t* object, aml_field_unit_obj_t* index, aml_field_unit_obj_t* data,
    aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || index == NULL || data == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
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

uint64_t aml_field_unit_bank_field_set(aml_object_t* object, aml_opregion_obj_t* opregion, aml_field_unit_obj_t* bank,
    uint64_t bankValue, aml_field_flags_t flags, aml_bit_size_t bitOffset, aml_bit_size_t bitSize)
{
    if (object == NULL || opregion == NULL || bank == NULL || bitSize == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->fieldUnit.fieldType = AML_FIELD_UNIT_BANK_FIELD;
    object->fieldUnit.index = NULL;
    object->fieldUnit.data = NULL;

    aml_object_t* bankValueObj = aml_object_new();
    if (bankValueObj == NULL)
    {
        return ERR;
    }
    if (aml_integer_set(bankValueObj, bankValue) == ERR)
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

uint64_t aml_integer_set(aml_object_t* object, aml_integer_t value)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type == AML_INTEGER)
    {
        object->integer.value = value & aml_integer_ones();
        return 0;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->integer.value = value & aml_integer_ones();
    object->type = AML_INTEGER;
    return 0;
}

uint64_t aml_method_set(aml_object_t* object, aml_method_flags_t flags, const uint8_t* start, const uint8_t* end,
    aml_method_implementation_t implementation)
{
    if (object == NULL || ((start == 0 || end == 0 || start > end) && implementation == NULL))
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->method.implementation = implementation;
    object->method.methodFlags = flags;
    object->method.start = start;
    object->method.end = end;
    aml_container_init(&object->method.container);
    aml_mutex_id_init(&object->method.mutex);
    object->type = AML_METHOD;
    return 0;
}

static inline aml_method_obj_t* aml_method_find_recursive(aml_object_t* current, const uint8_t* addr)
{
    if (current == NULL || addr == NULL)
    {
        return NULL;
    }

    if (current->type == AML_METHOD)
    {
        if (addr >= current->method.start && addr < current->method.end)
        {
            return REF(&current->method);
        }
    }

    if (current->type & AML_CONTAINERS)
    {
        aml_container_t* container = aml_object_container_get(current);
        if (container != NULL)
        {
            aml_object_t* child = NULL;
            LIST_FOR_EACH(child, &container->namedObjects, name.parentEntry)
            {
                aml_method_obj_t* result = aml_method_find_recursive(child, addr);
                if (result != NULL)
                {
                    return result; // Transfer ownership
                }
            }
        }
    }

    return NULL;
}

aml_method_obj_t* aml_method_find(const uint8_t* addr)
{
    if (addr == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    aml_object_t* root = aml_root_get();
    if (root == NULL)
    {
        return NULL;
    }
    DEREF_DEFER(root);

    return aml_method_find_recursive(root, addr); // Transfer ownership
}

uint64_t aml_mutex_set(aml_object_t* object, aml_sync_level_t syncLevel)
{
    if (object == NULL || syncLevel > 15)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->mutex.syncLevel = syncLevel;
    aml_mutex_id_init(&object->mutex.mutex);
    object->type = AML_MUTEX;
    return 0;
}

uint64_t aml_object_reference_set(aml_object_t* object, aml_object_t* target)
{
    if (object == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->objectReference.target = REF(target);
    object->type = AML_OBJECT_REFERENCE;
    return 0;
}

uint64_t aml_operation_region_set(aml_object_t* object, aml_region_space_t space, uintptr_t offset, uint32_t length)
{
    if (object == NULL || length == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->opregion.space = space;
    object->opregion.offset = offset;
    object->opregion.length = length;
    object->type = AML_OPERATION_REGION;
    return 0;
}

uint64_t aml_package_set(aml_object_t* object, uint64_t length)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    if (length <= AML_SMALL_PACKAGE_SIZE)
    {
        object->package.elements = object->package.smallElements;
    }
    else
    {
        object->package.elements = heap_alloc(sizeof(aml_object_t*) * length, HEAP_NONE);
        if (object->package.elements == NULL)
        {
            return ERR;
        }
    }
    memset(object->package.elements, 0, sizeof(aml_object_t*) * length);

    for (uint64_t i = 0; i < length; i++)
    {
        object->package.elements[i] = aml_object_new();
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

uint64_t aml_power_resource_set(aml_object_t* object, aml_system_level_t systemLevel,
    aml_resource_order_t resourceOrder)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->powerResource.systemLevel = systemLevel;
    object->powerResource.resourceOrder = resourceOrder;
    aml_container_init(&object->powerResource.container);
    object->type = AML_POWER_RESOURCE;
    return 0;
}

uint64_t aml_processor_set(aml_object_t* object, aml_proc_id_t procId, aml_pblk_addr_t pblkAddr, aml_pblk_len_t pblkLen)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->processor.procId = procId;
    object->processor.pblkAddr = pblkAddr;
    object->processor.pblkLen = pblkLen;
    aml_container_init(&object->processor.container);
    object->type = AML_PROCESSOR;
    return 0;
}

uint64_t aml_string_set_empty(aml_object_t* object, uint64_t length)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    if (length <= AML_SMALL_STRING_SIZE)
    {
        object->string.content = object->string.smallString;
        memset(object->string.content, 0, length + 1);
        object->string.length = length;
        object->type = AML_STRING;
        return 0;
    }

    object->string.content = heap_alloc(length + 1, HEAP_NONE);
    if (object->string.content == NULL)
    {
        return ERR;
    }
    memset(object->string.content, 0, length + 1);
    object->string.length = length;
    object->type = AML_STRING;
    return 0;
}

uint64_t aml_string_set(aml_object_t* object, const char* str)
{
    if (object == NULL || str == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    size_t length = strlen(str);
    if (aml_string_set_empty(object, length) == ERR)
    {
        return ERR;
    }
    memcpy(object->string.content, str, length);
    return 0;
}

uint64_t aml_string_resize(aml_string_obj_t* string, uint64_t newLength)
{
    if (string == NULL || newLength == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    if (newLength == string->length)
    {
        return 0;
    }

    if (newLength <= AML_SMALL_STRING_SIZE && string->length <= AML_SMALL_STRING_SIZE)
    {
        if (newLength > string->length)
        {
            memset(string->content + string->length, 0, newLength - string->length);
        }
        string->content[newLength] = '\0';
        string->length = newLength;
        return 0;
    }

    if (newLength <= AML_SMALL_STRING_SIZE && string->length > AML_SMALL_STRING_SIZE)
    {
        memcpy(string->smallString, string->content, newLength);
        string->smallString[newLength] = '\0';
        heap_free(string->content);
        string->content = string->smallString;
        string->length = newLength;
        return 0;
    }

    if (newLength > AML_SMALL_STRING_SIZE && string->length <= AML_SMALL_STRING_SIZE)
    {
        char* newBuffer = heap_alloc(newLength + 1, HEAP_NONE);
        if (newBuffer == NULL)
        {
            return ERR;
        }
        memcpy(newBuffer, string->content, string->length);
        memset(newBuffer + string->length, 0, newLength - string->length);
        newBuffer[newLength] = '\0';
        string->content = newBuffer;
        string->length = newLength;
        return 0;
    }

    if (newLength > AML_SMALL_STRING_SIZE && string->length > AML_SMALL_STRING_SIZE)
    {
        char* newBuffer = heap_alloc(newLength + 1, HEAP_NONE);
        if (newBuffer == NULL)
        {
            return ERR;
        }
        size_t copyLen = (newLength < string->length ? newLength : string->length);
        memcpy(newBuffer, string->content, copyLen);
        if (newLength > string->length)
        {
            memset(newBuffer + string->length, 0, newLength - string->length);
        }
        newBuffer[newLength] = '\0';
        heap_free(string->content);
        string->content = newBuffer;
        string->length = newLength;
        return 0;
    }

    // Should never reach here
    errno = EINVAL;
    return ERR;
}

uint64_t aml_thermal_zone_set(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    aml_container_init(&object->thermalZone.container);
    object->type = AML_THERMAL_ZONE;
    return 0;
}

uint64_t aml_alias_set(aml_object_t* object, aml_object_t* target)
{
    if (object == NULL || target == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->alias.target = REF(target);
    object->type = AML_ALIAS;
    return 0;
}

aml_object_t* aml_alias_obj_traverse(aml_alias_obj_t* alias)
{
    if (alias == NULL)
    {
        return NULL;
    }

    aml_object_t* current = REF(CONTAINER_OF(alias, aml_object_t, alias));
    while (current != NULL && current->type == AML_ALIAS)
    {
        aml_object_t* next = REF(current->alias.target);
        if (next == NULL)
        {
            return NULL;
        }
        DEREF(current);
        current = next;
    }
    return current;
}

uint64_t aml_unresolved_set(aml_object_t* object, const aml_name_string_t* nameString, aml_object_t* from,
    aml_patch_up_resolve_callback_t callback)
{
    if (object == NULL || nameString == NULL || callback == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
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

uint64_t aml_predefined_scope_set(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    aml_container_init(&object->predefinedScope.container);
    object->type = AML_PREDEFINED_SCOPE;
    return 0;
}

uint64_t aml_arg_set(aml_object_t* object, aml_object_t* value)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    if (value == NULL)
    {
        object->arg.value = NULL;
        object->type = AML_ARG;
        return 0;
    }

    object->arg.value = REF(value);
    object->type = AML_ARG;
    return 0;
}

uint64_t aml_local_set(aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->local.value = aml_object_new();
    if (object->local.value == NULL)
    {
        return ERR;
    }
    object->type = AML_LOCAL;
    return 0;
}
