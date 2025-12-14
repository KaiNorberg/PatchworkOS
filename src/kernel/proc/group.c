#include <kernel/proc/group.h>
#include <kernel/utils/map.h>
#include <kernel/proc/process.h>

#include <errno.h>
#include <stdlib.h>

static map_t groups = MAP_CREATE();
static uint64_t nextGid = 1;
static lock_t groupsLock = LOCK_CREATE();

group_t* group_add(gid_t gid, process_t* process)
{
    if (process == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    LOCK_SCOPE(&groupsLock);

    if (gid != GID_NONE)
    {
        map_key_t key = map_key_uint64(gid);
        map_entry_t* entry = map_get(&groups, &key);
        if (entry == NULL)
        {
            errno = ENOENT;
            return NULL;
        }
        
        group_t* group = CONTAINER_OF(entry, group_t, mapEntry);
        lock_acquire(&group->lock);
        list_push_back(&group->processes, &process->groupEntry);
        lock_release(&group->lock);
        return group;
    }

    group_t* group = malloc(sizeof(group_t));
    if (group == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    group->id = nextGid++;
    list_init(&group->processes);
    lock_init(&group->lock);

    list_push_back(&group->processes, &process->groupEntry);
    map_key_t key = map_key_uint64(group->id);
    map_insert(&groups, &key, &group->mapEntry);
    return group;
}

void group_remove(group_t* group, process_t* process)
{
    if (group == NULL || process == NULL)
    {
        return;
    }

    lock_acquire(&groupsLock);
    lock_acquire(&group->lock);

    list_remove(&group->processes, &process->groupEntry);

    if (list_is_empty(&group->processes))
    {
        map_remove(&groups, &group->mapEntry);
        lock_release(&group->lock);
        free(group);
        return;
    }

    lock_release(&group->lock);
    lock_release(&groupsLock);
}