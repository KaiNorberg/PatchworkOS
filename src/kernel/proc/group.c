#include <_internal/CONTAINER_OF.h>
#include <kernel/proc/group.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>
#include <kernel/utils/map.h>

#include <errno.h>
#include <stdlib.h>

static map_t groups = MAP_CREATE();
static uint64_t nextGid = 1;
static lock_t groupsLock = LOCK_CREATE();

uint64_t group_add(gid_t gid, group_entry_t* entry)
{
    if (entry == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&groupsLock);

    if (entry->group != NULL)
    {
        errno = EBUSY;
        return ERR;
    }

    if (gid != GID_NONE)
    {
        map_key_t key = map_key_uint64(gid);
        group_t* group = CONTAINER_OF_SAFE(map_get(&groups, &key), group_t, mapEntry);
        if (group == NULL)
        {
            errno = ENOENT;
            return ERR;
        }

        entry->group = group;
        list_push_back(&group->processes, &entry->entry);
        return 0;
    }

    group_t* group = malloc(sizeof(group_t));
    if (group == NULL)
    {
        errno = ENOMEM;
        return ERR;
    }
    group->id = nextGid++;
    list_init(&group->processes);

    entry->group = group;
    list_push_back(&group->processes, &entry->entry);

    map_key_t key = map_key_uint64(group->id);
    map_insert(&groups, &key, &group->mapEntry);
    return 0;
}

void group_remove(group_entry_t* entry)
{
    if (entry == NULL)
    {
        return;
    }

    LOCK_SCOPE(&groupsLock);

    if (entry->group == NULL)
    {
        return;
    }

    list_remove(&entry->group->processes, &entry->entry);

    if (list_is_empty(&entry->group->processes))
    {
        map_remove(&groups, &entry->group->mapEntry);
        free(entry->group);
    }

    entry->group = NULL;
}

gid_t group_get_id(group_entry_t* entry)
{
    if (entry == NULL)
    {
        return GID_NONE;
    }

    LOCK_SCOPE(&groupsLock);

    if (entry->group == NULL)
    {
        return GID_NONE;
    }

    return entry->group->id;
}

uint64_t group_send_note(group_entry_t* entry, const char* note)
{
    if (entry == NULL || note == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&groupsLock);

    if (entry->group == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process;
    LIST_FOR_EACH(process, &entry->group->processes, groupEntry.entry)
    {
        LOCK_SCOPE(&process->threads.lock);

        thread_t* thread = CONTAINER_OF_SAFE(list_first(&process->threads.list), thread_t, processEntry);
        if (thread == NULL)
        {
            continue;
        }

        if (thread_send_note(thread, note) == ERR)
        {
            return ERR;
        }
    }

    return 0;
}