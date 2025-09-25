#include "aml_patch_up.h"

#include "aml.h"
#include "log/log.h"
#include "mem/heap.h"
#include "aml_to_string.h"

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

uint64_t aml_patch_up_resolve_all()
{
    mutex_t* globalMutex = aml_global_mutex_get();
    mutex_acquire_recursive(globalMutex);

    aml_patch_up_entry_t* entry = NULL;
    aml_patch_up_entry_t* temp = NULL;
    LIST_FOR_EACH_SAFE(entry, temp, &unresolvedNodes, entry)
    {
        LOG_DEBUG("Attempting to resolve patch up entry for '%s'\n",
            aml_name_string_to_string(&entry->unresolved->unresolved.nameString));
        aml_node_t* match =
            aml_name_string_resolve(&entry->unresolved->unresolved.nameString, entry->unresolved->unresolved.start);
        if (match == NULL)
        {
            LOG_DEBUG("Still could not resolve '%s'\n",
                aml_name_string_to_string(&entry->unresolved->unresolved.nameString));
            errno = 0;
            continue;
        }

        if (entry->callback(match, entry->unresolved) == ERR)
        {
            LOG_ERR(NULL, "Failed to resolve patch up entry");
            mutex_release(globalMutex);
            return ERR;
        }

        LOG_DEBUG("Resolved patch up entry for '%s'\n",
            aml_name_string_to_string(&entry->unresolved->unresolved.nameString));
        list_remove(&unresolvedNodes, &entry->entry);
        heap_free(entry);
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
