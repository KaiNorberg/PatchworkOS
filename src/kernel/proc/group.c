#include <kernel/fs/dentry.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/vfs.h>
#include <kernel/log/log.h>
#include <kernel/proc/group.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>

#include <errno.h>
#include <kernel/sync/lock.h>
#include <stdlib.h>

static void group_free(group_t* group)
{
    if (group == NULL)
    {
        return;
    }

    assert(list_is_empty(&group->processes));

    free(group);
}

static group_t* group_new(void)
{
    group_t* group = malloc(sizeof(group_t));
    if (group == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }
    ref_init(&group->ref, group_free);
    list_init(&group->processes);
    lock_init(&group->lock);

    return group;
}

uint64_t group_member_init(group_member_t* member, group_member_t* group)
{
    list_entry_init(&member->entry);
    member->group = NULL;
    lock_init(&member->lock);

    if (group != NULL)
    {
        group_t* grp = group_get(group);
        if (grp == NULL)
        {
            return ERR;
        }

        group_add(grp, member);
        return 0;
    }

    group_t* newGroup = group_new();
    if (newGroup == NULL)
    {
        return ERR;
    }

    lock_acquire(&newGroup->lock);
    list_push_back(&newGroup->processes, &member->entry);
    member->group = newGroup;
    lock_release(&newGroup->lock);

    return 0;
}

void group_member_deinit(group_member_t* member)
{
    group_remove(member);
}

group_t* group_get(group_member_t* member)
{
    if (member == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    LOCK_SCOPE(&member->lock);

    if (member->group == NULL)
    {
        errno = ESRCH;
        return NULL;
    }

    return REF(member->group);
}

void group_add(group_t* group, group_member_t* member)
{
    if (group == NULL || member == NULL)
    {
        return;
    }

    LOCK_SCOPE(&member->lock);
    if (member->group != NULL)
    {
        lock_acquire(&member->group->lock);
        list_remove(&member->entry);
        lock_release(&member->group->lock);
        UNREF(member->group);
        member->group = NULL;
    }

    LOCK_SCOPE(&group->lock);
    list_push_back(&group->processes, &member->entry);
    member->group = REF(group);
}

void group_remove(group_member_t* member)
{
    if (member == NULL)
    {
        return;
    }

    LOCK_SCOPE(&member->lock);

    if (member->group == NULL)
    {
        return;
    }

    lock_acquire(&member->group->lock);
    list_remove(&member->entry);
    lock_release(&member->group->lock);

    UNREF(member->group);
    member->group = NULL;
}

uint64_t group_send_note(group_member_t* member, const char* note)
{
    if (member == NULL || note == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    group_t* group = member->group;
    if (group == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    LOCK_SCOPE(&group->lock);

    process_t* process;
    LIST_FOR_EACH(process, &group->processes, group.entry)
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