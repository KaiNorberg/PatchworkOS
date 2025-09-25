#include "aml_patch_up.h"

#include "aml.h"
#include "aml_to_string.h"
#include "log/log.h"
#include "mem/heap.h"

#include <errno.h>
#include <sys/list.h>

static list_t unresolvedNodes;

void aml_patch_up_init(void)
{
    list_init(&unresolvedNodes);
}

uint64_t aml_patch_up_add_unresolved(aml_node_t* unresolved, aml_patch_up_resolve_callback_t callback)
{
    if (unresolved == NULL || unresolved->type != AML_DATA_UNRESOLVED || !unresolved->isAllocated)
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
    mutex_acquire_recursive(globalMutex);
    list_push(&unresolvedNodes, &entry->entry);
    mutex_release(globalMutex);

    return 0;
}

void aml_patch_up_remove_unresolved(aml_node_t* unresolved)
{
    if (unresolved == NULL || unresolved->type != AML_DATA_UNRESOLVED || !unresolved->isAllocated)
    {
        return;
    }

    mutex_t* globalMutex = aml_global_mutex_get();
    mutex_acquire_recursive(globalMutex);

    aml_patch_up_entry_t* entry = NULL;
    LIST_FOR_EACH(entry, &unresolvedNodes, entry)
    {
        if (entry->unresolved == unresolved)
        {
            list_remove(&unresolvedNodes, &entry->entry);
            heap_free(entry);
            break;
        }
    }

    mutex_release(globalMutex);
}

uint64_t aml_patch_up_resolve_all()
{
    mutex_t* globalMutex = aml_global_mutex_get();
    mutex_acquire_recursive(globalMutex);

    aml_patch_up_entry_t* entry = NULL;
    aml_patch_up_entry_t* temp = NULL;
    LIST_FOR_EACH_SAFE(entry, temp, &unresolvedNodes, entry)
    {
        aml_node_t* match =
            aml_name_string_resolve(&entry->unresolved->unresolved.nameString, entry->unresolved->unresolved.start);
        if (match == NULL)
        {
            LOG_DEBUG("Still could not resolve '%s'\n",
                aml_name_string_to_string(&entry->unresolved->unresolved.nameString));
            errno = 0;
            continue;
        }

        aml_node_t* unresolved = entry->unresolved;
        if (entry->callback(match, unresolved) == ERR)
        {
            LOG_ERR("Failed to patch up unresolved node\n");
            mutex_release(globalMutex);
            return ERR;
        }

        // When the unresolved node is initalized as somethine else, it will be removed from the list in `aml_node_deinit()` and the entry will be freed.
        // If it hasent been removed then something has gone wrong.

        if (unresolved->type == AML_DATA_UNRESOLVED)
        {
            LOG_ERR("Patch up callback did not initalize the unresolved node\n");
            mutex_release(globalMutex);
            return ERR;
        }
    }

    mutex_release(globalMutex);
    return 0;
}

uint64_t aml_patch_up_unresolved_count(void)
{
    mutex_t* globalMutex = aml_global_mutex_get();
    mutex_acquire_recursive(globalMutex);
    uint64_t count = list_length(&unresolvedNodes);
    mutex_release(globalMutex);
    return count;
}
