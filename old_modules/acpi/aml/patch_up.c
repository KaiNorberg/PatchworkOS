#include <kernel/acpi/aml/patch_up.h>

#include <kernel/acpi/aml/object.h>
#include <kernel/acpi/aml/state.h>
#include <kernel/acpi/aml/to_string.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/list.h>

static list_t unresolvedObjects;

status_t aml_patch_up_init(void)
{
    list_init(&unresolvedObjects);
    return OK;
}

status_t aml_patch_up_add_unresolved(aml_unresolved_t* unresolved)
{
    if (unresolved == NULL)
    {
        return ERR(ACPI, INVAL);
    }

    aml_patch_up_entry_t* entry = malloc(sizeof(aml_patch_up_entry_t));
    if (entry == NULL)
    {
        return ERR(ACPI, NOMEM);
    }

    list_entry_init(&entry->entry);
    entry->unresolved = unresolved;
    list_push_back(&unresolvedObjects, &entry->entry);
    return OK;
}

void aml_patch_up_remove_unresolved(aml_unresolved_t* unresolved)
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
            list_remove(&entry->entry);
            free(entry);
            return;
        }
    }
}

status_t aml_patch_up_resolve_all(void)
{
    aml_state_t state;
    status_t status = aml_state_init(&state, NULL);
    if (IS_ERR(status))
    {
        LOG_PANIC("Failed to init AML state\n");
        return status;
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
            continue;
        }

        aml_unresolved_t* unresolved = entry->unresolved;
        aml_object_t* obj = CONTAINER_OF(unresolved, aml_object_t, unresolved);
        status_t status = unresolved->callback(&state, match, obj);
        if (IS_ERR(status))
        {
            aml_state_deinit(&state);
            LOG_ERR("Failed to patch up unresolved object\n");
            return status;
        }

        // When a unresolved object changes type it will call aml_patch_up_remove_unresolved itself.
        if (obj->type == AML_UNRESOLVED)
        {
            aml_state_deinit(&state);
            LOG_ERR("Unresolved object did not change type\n");
            return ERR(ACPI, ILSEQ);
        }
    }

    aml_state_deinit(&state);
    return OK;
}

uint64_t aml_patch_up_unresolved_count(void)
{
    return list_size(&unresolvedObjects);
}
