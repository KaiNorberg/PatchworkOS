#include <kernel/acpi/aml/object.h>

#include <kernel/acpi/acpi.h>
#include <kernel/acpi/aml/aml.h>
#include <kernel/acpi/aml/exception.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/acpi/aml/token.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/math.h>

// Used to check for memory leaks
static uint64_t totalObjects = 0;

// Cache for aml_object_t to avoid frequent allocations
static list_t objectsCache = LIST_CREATE(objectsCache);

static aml_object_id_t newObjectId = AML_OBJECT_ID_NONE + 1;

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

    // Named objects should never be able to be freed while still being named as the parent would still have a reference
    // to them, unless the object is root.
    if ((object->flags & AML_OBJECT_NAMED) && !(object->flags & AML_OBJECT_ROOT))
    {
        panic(NULL, "Attempted to free named non-root AML object '%s'", AML_NAME_TO_STRING(object->name));
    }

    aml_object_clear(object);

    if (object->dir != NULL)
    {
        DEREF(object->dir);
        object->dir = NULL;
    }

    if (list_length(&objectsCache) < AML_OBJECT_CACHE_SIZE)
    {
        list_push(&objectsCache, &object->listEntry);
    }
    else
    {
        free(object);
    }

    totalObjects--;
}

aml_object_t* aml_object_new(void)
{
    aml_object_t* object = NULL;
    if (!list_is_empty(&objectsCache))
    {
        object = CONTAINER_OF_SAFE(list_pop(&objectsCache), aml_object_t, listEntry);
    }

    if (object == NULL)
    {
        object = calloc(1, sizeof(aml_object_t));
        if (object == NULL)
        {
            return NULL;
        }
    }

    ref_init(&object->ref, aml_object_free);
    object->id = newObjectId++;
    object->name = AML_NAME_UNDEFINED;
    map_entry_init(&object->mapEntry);
    list_entry_init(&object->listEntry);
    object->overlay = NULL;
    list_init(&object->children);
    list_entry_init(&object->siblingsEntry);
    object->parent = NULL;
    object->flags = AML_OBJECT_NONE;
    object->type = AML_UNINITIALIZED;
    object->dir = NULL;

    totalObjects++;
    return object;
}

void aml_object_clear(aml_object_t* object)
{
    if (object == NULL)
    {
        return;
    }

    if (object->flags & AML_OBJECT_NAMED)
    {
        panic(NULL, "Attempted to clear named AML object '%s'", AML_NAME_TO_STRING(object->name));
    }

    if (object->type == AML_UNINITIALIZED)
    {
        return;
    }

    if (object->type & AML_NAMESPACES)
    {
        aml_object_t* child;
        aml_object_t* temp;
        LIST_FOR_EACH_SAFE(child, temp, &object->children, siblingsEntry)
        {
            aml_namespace_remove(child);
        }
    }

    switch (object->type)
    {
    case AML_BUFFER:
        if (object->buffer.content != NULL && object->buffer.length > AML_SMALL_BUFFER_SIZE)
        {
            free(object->buffer.content);
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
                free(object->package.elements);
            }
        }
        object->package.elements = NULL;
        object->package.length = 0;
        break;
    case AML_POWER_RESOURCE:
        object->powerResource.systemLevel = 0;
        object->powerResource.resourceOrder = 0;
        break;
    case AML_PROCESSOR:
        object->processor.procId = 0;
        object->processor.pblkAddr = 0;
        object->processor.pblkLen = 0;
        break;
    case AML_STRING:
        if (object->string.content != NULL && object->string.length > AML_SMALL_STRING_SIZE)
        {
            free(object->string.content);
        }
        object->string.content = NULL;
        object->string.length = 0;
        break;
    case AML_THERMAL_ZONE:
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

uint64_t aml_object_count_children(aml_object_t* parent)
{
    if (parent == NULL)
    {
        return 0;
    }

    uint64_t count = 0;

    if (parent->type & AML_NAMESPACES)
    {
        aml_object_t* child = NULL;
        LIST_FOR_EACH(child, &parent->children, siblingsEntry)
        {
            count++;
            count += aml_object_count_children(child);
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

void aml_object_exception_check(aml_object_t* object, aml_state_t* state)
{
    if (object->flags & AML_OBJECT_EXCEPTION_ON_USE)
    {
        AML_EXCEPTION_RAISE(state, AML_PARSE); // Not fatal.
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

uint64_t aml_buffer_resize(aml_buffer_obj_t* buffer, uint64_t newLength)
{
    if (buffer == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (newLength <= AML_SMALL_BUFFER_SIZE)
    {
        if (buffer->content != buffer->smallBuffer)
        {
            memcpy(buffer->smallBuffer, buffer->content, MIN(buffer->length, newLength));
            free(buffer->content);
            buffer->content = buffer->smallBuffer;
        }
        buffer->length = newLength;
        return 0;
    }

    if (buffer->content == buffer->smallBuffer)
    {
        buffer->content = calloc(1, newLength);
        if (buffer->content == NULL)
        {
            return ERR;
        }
        memcpy(buffer->content, buffer->smallBuffer, buffer->length);
    }
    else
    {
        uint8_t* newContent = realloc(buffer->content, newLength);
        if (newContent == NULL)
        {
            return ERR;
        }
        buffer->content = newContent;
        memset(&buffer->content[buffer->length], 0, newLength - buffer->length);
    }
    buffer->length = newLength;
    return 0;
}

uint64_t aml_buffer_set_empty(aml_object_t* object, uint64_t length)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type == AML_BUFFER)
    {
        if (aml_buffer_resize(&object->buffer, length) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_object_check_clear(object) == ERR)
        {
            return ERR;
        }

        if (length <= AML_SMALL_BUFFER_SIZE)
        {
            object->buffer.content = object->buffer.smallBuffer;
            memset(object->buffer.content, 0, AML_SMALL_BUFFER_SIZE);
        }
        else
        {
            object->buffer.content = calloc(1, length);
            if (object->buffer.content == NULL)
            {
                return ERR;
            }
        }
        object->buffer.length = length;
    }

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
    memset(&object->buffer.content[bytesToCopy], 0, length - bytesToCopy);
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

    if (object->type != AML_BUFFER_FIELD)
    {
        if (aml_object_check_clear(object) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        DEREF(object->bufferField.target);
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

    aml_object_t* bankValueObj = aml_object_new();
    if (bankValueObj == NULL)
    {
        return ERR;
    }
    DEREF_DEFER(bankValueObj);
    if (aml_integer_set(bankValueObj, bankValue) == ERR)
    {
        return ERR;
    }

    if (aml_object_check_clear(object) == ERR)
    {
        return ERR;
    }

    object->fieldUnit.fieldType = AML_FIELD_UNIT_BANK_FIELD;
    object->fieldUnit.index = NULL;
    object->fieldUnit.data = NULL;
    object->fieldUnit.bankValue = REF(bankValueObj);
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

    if (current->type & AML_NAMESPACES)
    {
        aml_object_t* child = NULL;
        LIST_FOR_EACH(child, &current->children, siblingsEntry)
        {
            aml_method_obj_t* result = aml_method_find_recursive(child, addr);
            if (result != NULL)
            {
                // Check for methods in methods
                aml_method_obj_t* methodInMethod =
                    aml_method_find_recursive(CONTAINER_OF_SAFE(result, aml_object_t, method), addr);
                if (methodInMethod != NULL)
                {
                    DEREF(result);
                    return methodInMethod; // Transfer ownership
                }
                return result; // Transfer ownership
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

    aml_object_t* root = aml_namespace_get_root();
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

    if (object->type == AML_OBJECT_REFERENCE)
    {
        DEREF(object->objectReference.target);
        object->objectReference.target = REF(target);
        return 0;
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
        object->package.elements = malloc(sizeof(aml_object_t*) * length);
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
            free(object->package.elements);
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

    if (object->type == AML_STRING)
    {
        if (aml_string_resize(&object->string, length) == ERR)
        {
            return ERR;
        }
    }
    else
    {
        if (aml_object_check_clear(object) == ERR)
        {
            return ERR;
        }

        if (length <= AML_SMALL_STRING_SIZE)
        {
            object->string.content = object->string.smallString;
            memset(object->string.smallString, 0, AML_SMALL_STRING_SIZE);
        }
        else
        {
            object->string.content = calloc(1, length + 1);
            if (object->string.content == NULL)
            {
                return ERR;
            }
        }
        object->string.length = length;
    }

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
    if (string == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (newLength == string->length)
    {
        return 0;
    }

    if (newLength <= AML_SMALL_STRING_SIZE)
    {
        if (string->content != string->smallString)
        {
            memcpy(string->smallString, string->content, MIN(string->length, newLength));
            free(string->content);
            string->content = string->smallString;
        }
        string->length = newLength;
        string->smallString[newLength] = '\0';
        return 0;
    }

    if (string->content == string->smallString)
    {
        string->content = calloc(1, newLength + 1);
        if (string->content == NULL)
        {
            return ERR;
        }
        memcpy(string->content, string->smallString, MIN(string->length, newLength));
    }
    else
    {
        char* newContent = realloc(string->content, newLength + 1);
        if (newContent == NULL)
        {
            return ERR;
        }
        string->content = newContent;
        memset(&string->content[string->length], 0, newLength - string->length + 1);
    }

    string->length = newLength;
    string->content[newLength] = '\0';
    return 0;

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
