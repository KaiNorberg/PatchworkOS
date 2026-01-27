#include <kernel/fs/dentry.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/log/log.h>
#include <kernel/proc/group.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>

#include <kernel/sync/lock.h>
#include <kernel/sync/rcu.h>
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
        return NULL;
    }
    ref_init(&group->ref, group_free);
    list_init(&group->processes);
    lock_init(&group->lock);

    return group;
}

status_t group_member_init(group_member_t* member, group_member_t* group)
{
    list_entry_init(&member->entry);
    member->group = NULL;
    lock_init(&member->lock);

    if (group != NULL)
    {
        group_t* grp = group_get(group);
        if (grp == NULL)
        {
            return ERR(PROC, NOGROUP);
        }

        group_add(grp, member);
        return 0;
    }

    group_t* newGroup = group_new();
    if (newGroup == NULL)
    {
        return ERR(PROC, NOMEM);
    }

    lock_acquire(&newGroup->lock);
    list_push_back(&newGroup->processes, &member->entry);
    member->group = newGroup;
    lock_release(&newGroup->lock);

    return OK;
}

void group_member_deinit(group_member_t* member)
{
    group_remove(member);
}

group_t* group_get(group_member_t* member)
{
    if (member == NULL)
    {
        return NULL;
    }

    LOCK_SCOPE(&member->lock);
    if (member->group == NULL)
    {
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

status_t group_send_note(group_member_t* member, const char* note)
{
    if (member == NULL || note == NULL)
    {
        return ERR(PROC, INVAL);
    }

    group_t* group = member->group;
    if (group == NULL)
    {
        return ERR(PROC, NOGROUP);
    }

    LOCK_SCOPE(&group->lock);

    process_t* process;
    LIST_FOR_EACH(process, &group->processes, group.entry)
    {
        RCU_READ_SCOPE();

        thread_t* thread = process_rcu_first_thread(process);
        if (thread == NULL)
        {
            continue;
        }

        status_t status = thread_send_note(thread, note);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    return OK;
}