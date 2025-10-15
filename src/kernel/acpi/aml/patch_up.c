#include "patch_up.h"

#include "log/log.h"
#include "log/panic.h"
#include "mem/heap.h"
#include "object.h"
#include "state.h"
#include "to_string.h"

#include <errno.h>
#include <sys/list.h>

static list_t unresolvedObjects;

uint64_t aml_patch_up_init(void)
{
    list_init(&unresolvedObjects);
    return 0;
}

uint64_t aml_patch_up_add_unresolved(aml_unresolved_obj_t* unresolved)
{
    if (unresolved == NULL)
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
    list_push(&unresolvedObjects, &entry->entry);
    return 0;
}

void aml_patch_up_remove_unresolved(aml_unresolved_obj_t* unresolved)
{
    if (unresolved == NULL)
    {
        return;
    }

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
}

uint64_t aml_patch_up_resolve_all(void)
{
    aml_state_t state;
    if (aml_state_init(&state, NULL) == ERR)
    {
        LOG_PANIC("Failed to init AML state\n");
        return ERR;
    }

    aml_patch_up_entry_t* entry = NULL;
    aml_patch_up_entry_t* temp = NULL;
    LIST_FOR_EACH_SAFE(entry, temp, &unresolvedObjects, entry)
    {
        aml_object_t* match =
            aml_namespace_find_by_name_string(&state.overlay, entry->unresolved->from, &entry->unresolved->nameString);
        if (match == NULL)
        {
            LOG_DEBUG("Still could not resolve '%s'\n", aml_name_string_to_string(&entry->unresolved->nameString));
            errno = 0;
            continue;
        }

        aml_unresolved_obj_t* unresolved = entry->unresolved;
        aml_object_t* obj = CONTAINER_OF(unresolved, aml_object_t, unresolved);
        if (unresolved->callback(&state, match, obj) == ERR)
        {
            aml_state_deinit(&state);
            LOG_ERR("Failed to patch up unresolved object\n");
            return ERR;
        }

        // When a unresolved object changes type it will call aml_patch_up_remove_unresolved itself.
        if (obj->type == AML_UNRESOLVED)
        {
            aml_state_deinit(&state);
            LOG_ERR("Unresolved object did not change type\n");
            errno = EILSEQ;
            return ERR;
        }
    }

    aml_state_deinit(&state);
    return 0;
}

uint64_t aml_patch_up_unresolved_count(void)
{
    return list_length(&unresolvedObjects);
}
