#include "aml_patch_up.h"

#include "aml.h"
#include "aml_to_string.h"
#include "log/panic.h"
#include "log/log.h"
#include "mem/heap.h"

#include <errno.h>
#include <sys/list.h>

static list_t unresolvedObjects;

uint64_t aml_patch_up_init(void)
{
    list_init(&unresolvedObjects);
    return 0;
}

uint64_t aml_patch_up_add_unresolved(aml_object_t* unresolved, aml_patch_up_resolve_callback_t callback)
{
    if (unresolved == NULL || unresolved->type != AML_DATA_UNRESOLVED || unresolved->flags & AML_OBJECT_NAMED)
    {
        errno = EINVAL;
        return ERR;
    }

    aml_patch_up_entry_t* entry = heap_alloc(sizeof(aml_patch_up_entry_t), HEAP_NONE);
    if (entry == NULL)
    {
        return ERR;
    }

    list_entry_init(&entry->entry);
    entry->unresolved = unresolved;
    entry->callback = callback;

    mutex_t* globalMutex = aml_global_mutex_get();
    MUTEX_SCOPE(globalMutex);
    list_push(&unresolvedObjects, &entry->entry);
    return 0;
}

void aml_patch_up_remove_unresolved(aml_object_t* unresolved)
{
    if (unresolved == NULL || unresolved->type != AML_DATA_UNRESOLVED || unresolved->flags & AML_OBJECT_NAMED)
    {
        return;
    }

    mutex_t* globalMutex = aml_global_mutex_get();
    MUTEX_SCOPE(globalMutex);

    aml_patch_up_entry_t* entry = NULL;
    LIST_FOR_EACH(entry, &unresolvedObjects, entry)
    {
        if (entry->unresolved == unresolved)
        {
            list_remove(&unresolvedObjects, &entry->entry);
            heap_free(entry);
            return;
        }
    }

    panic(NULL, "Unresolved object not found in patch up list\n");
}

uint64_t aml_patch_up_resolve_all()
{
    mutex_t* globalMutex = aml_global_mutex_get();
    MUTEX_SCOPE(globalMutex);

    aml_patch_up_entry_t* entry = NULL;
    aml_patch_up_entry_t* temp = NULL;
    LIST_FOR_EACH_SAFE(entry, temp, &unresolvedObjects, entry)
    {
        aml_object_t* match =
            aml_name_string_resolve(&entry->unresolved->unresolved.nameString, entry->unresolved->unresolved.start);
        if (match == NULL)
        {
            LOG_DEBUG("Still could not resolve '%s'\n",
                aml_name_string_to_string(&entry->unresolved->unresolved.nameString));
            errno = 0;
            continue;
        }

        aml_object_t* unresolved = entry->unresolved;
        if (entry->callback(match, unresolved) == ERR)
        {
            LOG_ERR("Failed to patch up unresolved object\n");
            return ERR;
        }

        // When the unresolved object is initalized as somethine else, it will be removed from the list in
        // `aml_object_deinit()` and the entry will be freed. If it hasent been removed then something has gone wrong.

        if (unresolved->type == AML_DATA_UNRESOLVED)
        {
            LOG_ERR("Patch up callback did not initalize the unresolved object\n");
            return ERR;
        }
    }

    return 0;
}

uint64_t aml_patch_up_unresolved_count(void)
{
    mutex_t* globalMutex = aml_global_mutex_get();
    MUTEX_SCOPE(globalMutex);
    return list_length(&unresolvedObjects);
}
