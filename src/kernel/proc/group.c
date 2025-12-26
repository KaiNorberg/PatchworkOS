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

void group_member_init(group_member_t* member)
{
    list_entry_init(&member->entry);
    member->group = NULL;
}

void group_member_deinit(group_member_t* member)
{
    group_remove(member);
}

uint64_t group_add(gid_t gid, group_member_t* member)
{
    if (member == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&groupsLock);

    if (member->group != NULL)
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

        member->group = group;
        list_push_back(&group->processes, &member->entry);
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

    member->group = group;
    list_push_back(&group->processes, &member->entry);

    map_key_t key = map_key_uint64(group->id);
    map_insert(&groups, &key, &group->mapEntry);
    return 0;
}

void group_remove(group_member_t* member)
{
    if (member == NULL)
    {
        return;
    }

    LOCK_SCOPE(&groupsLock);

    if (member->group == NULL)
    {
        return;
    }

    list_remove(&member->group->processes, &member->entry);

    if (list_is_empty(&member->group->processes))
    {
        map_remove(&groups, &member->group->mapEntry);
        free(member->group);
    }

    member->group = NULL;
}

gid_t group_get_id(group_member_t* member)
{
    if (member == NULL)
    {
        return GID_NONE;
    }

    LOCK_SCOPE(&groupsLock);

    if (member->group == NULL)
    {
        return GID_NONE;
    }

    return member->group->id;
}

uint64_t group_send_note(group_member_t* member, const char* note)
{
    if (member == NULL || note == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&groupsLock);

    if (member->group == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    process_t* process;
    LIST_FOR_EACH(process, &member->group->processes, group.entry)
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