#include <modules/acpi/aml/namespace.h>

#include <kernel/fs/sysfs.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/utils/map.h>
#include <modules/acpi/acpi.h>
#include <modules/acpi/aml/aml.h>
#include <modules/acpi/aml/object.h>
#include <modules/acpi/aml/to_string.h>

#include <errno.h>

static aml_overlay_t globalOverlay;

static aml_object_t* namespaceRoot = NULL;

static dentry_t* namespaceDir = NULL;

static inline map_key_t aml_object_map_key(aml_object_id_t parentId, aml_name_t name)
{
    // Pack the parentId and name into a single buffer for the map key
    struct PACKED
    {
        aml_object_id_t parentId;
        aml_name_t name;
    } buffer;
    buffer.parentId = parentId;
    buffer.name = name;
    return map_key_buffer(&buffer, sizeof(buffer));
}

static inline aml_object_t* aml_namespace_traverse_parents(aml_object_t* current, uint64_t depth)
{
    for (uint64_t i = 0; i < depth; i++)
    {
        if (current == NULL || current->parent == NULL)
        {
            UNREF(current);
            return NULL;
        }
        aml_object_t* parent = REF(current->parent);
        UNREF(current);
        current = parent;
    }
    return current;
}

static inline aml_object_t* aml_namespace_search_single_name(aml_overlay_t* overlay, aml_object_t* current,
    aml_name_t name)
{
    aml_object_t* next = aml_namespace_find_child(overlay, current, name);

    if (next == NULL)
    {
        while (current->parent != NULL)
        {
            aml_object_t* parent = REF(current->parent);
            UNREF(current);
            current = parent;

            next = aml_namespace_find_child(overlay, current, name);
            if (next != NULL)
            {
                break;
            }
        }
    }

    UNREF(current);
    return next;
}

void aml_namespace_init(aml_object_t* root)
{
    aml_overlay_init(&globalOverlay);
    root->flags |= AML_OBJECT_NAMED | AML_OBJECT_ROOT;
    root->name = AML_NAME('\\', '_', '_', '_');
    namespaceRoot = REF(root);
}

static uint64_t aml_namespace_expose_object(aml_object_t* object, dentry_t* parentDir)
{
    if (object == NULL || parentDir == NULL)
    {
        return ERR;
    }

    if (!(object->flags & AML_OBJECT_NAMED))
    {
        // Something is very wrong if we have an unnamed object in the namespace heirarchy
        LOG_ERR("unnamed object %s of type %s found in the namespace heirarchy\n", AML_NAME_TO_STRING(object->name),
            aml_type_to_string(object->type));
        return ERR;
    }

    object->dir = sysfs_dir_new(parentDir, AML_NAME_TO_STRING(object->name), NULL, NULL);
    if (object->dir == NULL)
    {
        LOG_ERR("Failed to create sysfs directory %s\n", AML_NAME_TO_STRING(object->name));
        return ERR;
    }

    if (object->type & AML_NAMESPACES)
    {
        aml_object_t* child = NULL;
        LIST_FOR_EACH(child, &object->children, siblingsEntry)
        {
            if (aml_namespace_expose_object(child, object->dir) == ERR)
            {
                LOG_ERR("Failed to expose child %s of %s in sysfs\n", AML_NAME_TO_STRING(child->name),
                    AML_NAME_TO_STRING(object->name));
                return ERR;
            }
        }
    }

    return 0;
}

uint64_t aml_namespace_expose(void)
{
    dentry_t* acpiDir = acpi_get_dir();
    assert(acpiDir != NULL);

    namespaceDir = sysfs_dir_new(acpiDir, "namespace", NULL, NULL);
    UNREF(acpiDir);
    if (namespaceDir == NULL)
    {
        LOG_ERR("Failed to create ACPI namespace sysfs directory");
        return ERR;
    }

    aml_object_t* child = NULL;
    LIST_FOR_EACH(child, &namespaceRoot->children, siblingsEntry)
    {
        if (aml_namespace_expose_object(child, namespaceDir) == ERR)
        {
            UNREF(namespaceDir);
            namespaceDir = NULL;
            LOG_ERR("Failed to expose ACPI namespace in sysfs");
            return ERR;
        }
    }

    return 0;
}

aml_object_t* aml_namespace_get_root(void)
{
    return REF(namespaceRoot);
}

aml_object_t* aml_namespace_find_child(aml_overlay_t* overlay, aml_object_t* parent, aml_name_t name)
{
    if (parent == NULL || !(parent->flags & AML_OBJECT_NAMED))
    {
        return NULL;
    }

    if (overlay == NULL)
    {
        overlay = &globalOverlay;
    }

    map_key_t key = aml_object_map_key(parent->id, name);
    aml_object_t* child = NULL;
    while (overlay != NULL)
    {
        child = CONTAINER_OF_SAFE(map_get(&overlay->map, &key), aml_object_t, mapEntry);
        if (child != NULL)
        {
            break;
        }

        overlay = overlay->parent;
    }

    if (child == NULL)
    {
        return NULL;
    }

    if (child->type == AML_ALIAS)
    {
        return aml_alias_traverse(&child->alias);
    }

    return REF(child);
}

aml_object_t* aml_namespace_find(aml_overlay_t* overlay, aml_object_t* start, uint64_t nameCount, ...)
{
    if (nameCount == 0)
    {
        return NULL;
    }

    aml_object_t* current = start != NULL ? REF(start) : REF(namespaceRoot);
    if (current == NULL || !(current->flags & AML_OBJECT_NAMED))
    {
        UNREF(current);
        return NULL;
    }

    if (nameCount == 1)
    {
        va_list args;
        va_start(args, nameCount);
        aml_name_t name = va_arg(args, aml_name_t);
        va_end(args);

        return aml_namespace_search_single_name(overlay, current, name);
    }

    va_list args;
    va_start(args, nameCount);
    for (uint64_t i = 0; i < nameCount; i++)
    {
        aml_name_t name = va_arg(args, aml_name_t);
        aml_object_t* next = aml_namespace_find_child(overlay, current, name);

        if (next == NULL)
        {
            UNREF(current);
            va_end(args);
            return NULL;
        }

        UNREF(current);
        current = next;
    }
    va_end(args);
    return current; // Transfer ownership
}

aml_object_t* aml_namespace_find_by_name_string(aml_overlay_t* overlay, aml_object_t* start,
    const aml_name_stioring_t* nameString)
{
    if (nameString == NULL)
    {
        return NULL;
    }

    aml_object_t* current = (start == NULL || nameString->rootChar.present) ? REF(namespaceRoot) : REF(start);
    if (!(current->flags & AML_OBJECT_NAMED))
    {
        UNREF(current);
        return NULL;
    }

    current = aml_namespace_traverse_parents(current, nameString->prefixPath.depth);
    if (current == NULL)
    {
        return NULL;
    }

    if (!nameString->rootChar.present && nameString->prefixPath.depth == 0 && nameString->namePath.segmentCount == 1)
    {
        aml_name_t name = nameString->namePath.segments[0];
        return aml_namespace_search_single_name(overlay, current, name);
    }

    for (uint64_t i = 0; i < nameString->namePath.segmentCount; i++)
    {
        if (!(current->flags & AML_OBJECT_NAMED))
        {
            UNREF(current);
            return NULL;
        }

        aml_name_t name = nameString->namePath.segments[i];
        aml_object_t* next = aml_namespace_find_child(overlay, current, name);

        if (next == NULL)
        {
            UNREF(current);
            return NULL;
        }

        UNREF(current);
        current = next;
    }

    return current; // Transfer ownership
}

aml_object_t* aml_namespace_find_by_path(aml_overlay_t* overlay, aml_object_t* start, const char* path)
{
    if (path == NULL || path[0] == '\0')
    {
        errno = EINVAL;
        return NULL;
    }

    aml_object_t* current = NULL;
    const char* p = path;
    if (p[0] == '\\')
    {
        current = REF(namespaceRoot);
        p++;
    }
    else if (p[0] == '^')
    {
        if (start == NULL)
        {
            errno = EINVAL;
            return NULL;
        }

        uint64_t depth = 0;
        while (p[0] == '^')
        {
            depth++;
            p++;
        }

        current = aml_namespace_traverse_parents(REF(start), depth);
    }
    else
    {
        current = start != NULL ? REF(start) : REF(namespaceRoot);
    }

    if (current == NULL || !(current->flags & AML_OBJECT_NAMED))
    {
        UNREF(current);
        return NULL;
    }

    if (*p == '\0')
    {
        return current; // Transfer ownership
    }

    uint64_t segmentCount = 0;
    while (*p != '\0')
    {
        const char* segmentStart = p;
        while (*p != '.' && *p != '\0')
        {
            p++;
        }

        uint64_t segmentLength = p - segmentStart;
        if (segmentLength > sizeof(aml_name_t))
        {
            UNREF(current);
            return NULL;
        }

        aml_name_t segment = 0;
        for (uint64_t i = 0; i < segmentLength; i++)
        {
            segment |= ((aml_name_t)(uint8_t)segmentStart[i]) << (i * 8);
        }

        segmentCount++;

        if (*p == '.')
        {
            p++;
        }

        aml_object_t* next = aml_namespace_find_child(overlay, current, segment);
        if (next == NULL && segmentCount == 1 && *p == '\0')
        {
            while (current->parent != NULL)
            {
                aml_object_t* parent = REF(current->parent);
                UNREF(current);
                current = parent;

                next = aml_namespace_find_child(overlay, current, segment);
                if (next != NULL)
                {
                    break;
                }
            }
        }

        if (next == NULL)
        {
            UNREF(current);
            return NULL;
        }

        UNREF(current);
        current = next;
    }

    return current; // Transfer ownership
}

uint64_t aml_namespace_add_child(aml_overlay_t* overlay, aml_object_t* parent, aml_name_t name, aml_object_t* object)
{
    if (object == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (object->type == AML_UNINITIALIZED)
    {
        errno = EINVAL;
        return ERR;
    }

    parent = parent != NULL ? parent : namespaceRoot;
    if (!(parent->flags & AML_OBJECT_NAMED) || (object->flags & AML_OBJECT_NAMED))
    {
        errno = EINVAL;
        return ERR;
    }

    if (overlay == NULL)
    {
        overlay = &globalOverlay;
    }

    map_key_t key = aml_object_map_key(parent->id, name);
    aml_overlay_t* currentOverlay = overlay;
    while (currentOverlay != NULL)
    {
        if (map_get(&currentOverlay->map, &key) != NULL)
        {
            errno = EEXIST;
            return ERR;
        }
        currentOverlay = currentOverlay->parent;
    }

    if (map_insert(&overlay->map, &key, &object->mapEntry) == ERR)
    {
        return ERR;
    }
    list_push_back(&overlay->objects, &object->listEntry);
    list_push_back(&parent->children, &object->siblingsEntry);

    object->flags |= AML_OBJECT_NAMED;
    object->overlay = overlay;
    object->parent = parent;
    object->name = name;

    REF(object);
    return 0;
}

uint64_t aml_namespace_add_by_name_string(aml_overlay_t* overlay, aml_object_t* start,
    const aml_name_stioring_t* nameString, aml_object_t* object)
{
    if (nameString == NULL || nameString->namePath.segmentCount == 0)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_name_t targetName = nameString->namePath.segments[nameString->namePath.segmentCount - 1];
    if (nameString->namePath.segmentCount == 1)
    {
        aml_object_t* parent = (start == NULL || nameString->rootChar.present) ? REF(namespaceRoot) : REF(start);

        parent = aml_namespace_traverse_parents(parent, nameString->prefixPath.depth);
        if (parent == NULL)
        {
            return ERR;
        }
        UNREF_DEFER(parent);

        return aml_namespace_add_child(overlay, parent, targetName, object);
    }

    aml_name_stioring_t parentNameString = *nameString;
    parentNameString.namePath.segmentCount--;

    aml_object_t* parent = aml_namespace_find_by_name_string(overlay, start, &parentNameString);
    if (parent == NULL)
    {
        errno = ENOENT;
        return ERR;
    }
    UNREF_DEFER(parent);

    return aml_namespace_add_child(overlay, parent, targetName, object);
}

void aml_namespace_remove(aml_object_t* object)
{
    if (object == NULL || !(object->flags & AML_OBJECT_NAMED))
    {
        return;
    }

    map_remove(&object->overlay->map, &object->mapEntry);
    list_remove(&object->listEntry);
    list_remove(&object->siblingsEntry);

    object->overlay = NULL;
    object->parent = NULL;
    object->flags &= ~AML_OBJECT_NAMED;
    object->name = AML_NAME_UNDEFINED;

    UNREF(object);
}

uint64_t aml_namespace_commit(aml_overlay_t* overlay)
{
    if (overlay == NULL || overlay->parent == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_object_t* object;
    aml_object_t* temp;
    LIST_FOR_EACH_SAFE(object, temp, &overlay->objects, listEntry)
    {
        map_remove(&overlay->map, &object->mapEntry);

        map_key_t key =
            aml_object_map_key(object->parent != NULL ? object->parent->id : AML_OBJECT_ID_NONE, object->name);
        if (map_insert(&overlay->parent->map, &key, &object->mapEntry) == ERR)
        {
            return ERR;
        }

        list_remove(&object->listEntry);
        list_push_back(&overlay->parent->objects, &object->listEntry);

        object->overlay = overlay->parent;
    }

    assert(list_is_empty(&overlay->objects));
    assert(map_is_empty(&overlay->map));

    return 0;
}

void aml_overlay_init(aml_overlay_t* overlay)
{
    if (overlay == NULL)
    {
        return;
    }

    map_init(&overlay->map);
    list_init(&overlay->objects);
    overlay->parent = overlay != &globalOverlay ? &globalOverlay : NULL;
}

void aml_overlay_deinit(aml_overlay_t* overlay)
{
    if (overlay == NULL)
    {
        return;
    }

    while (!list_is_empty(&overlay->objects))
    {
        aml_object_t* obj = CONTAINER_OF(list_pop_front(&overlay->objects), aml_object_t, listEntry);
        aml_namespace_remove(obj);
    }

    map_deinit(&overlay->map);
}

void aml_overlay_set_parent(aml_overlay_t* overlay, aml_overlay_t* parent)
{
    if (overlay == NULL)
    {
        return;
    }

    overlay->parent = parent;
}

aml_overlay_t* aml_overlay_find_containing(aml_overlay_t* overlay, aml_object_t* object)
{
    if (overlay == NULL || object == NULL)
    {
        return NULL;
    }

    aml_object_id_t parentId = object->parent != NULL ? object->parent->id : AML_OBJECT_ID_NONE;
    map_key_t key = aml_object_map_key(parentId, object->name);

    aml_overlay_t* currentOverlay = overlay;
    while (currentOverlay != NULL)
    {
        if (map_get(&currentOverlay->map, &key) != NULL)
        {
            return currentOverlay;
        }

        currentOverlay = currentOverlay->parent;
    }

    return NULL;
}
