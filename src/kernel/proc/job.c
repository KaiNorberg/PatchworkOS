#include <kernel/fs/dentry.h>
#include <kernel/fs/filesystem.h>
#include <kernel/fs/vfs.h>
#include <kernel/fs/vnode.h>
#include <kernel/log/log.h>
#include <kernel/proc/job.h>
#include <kernel/proc/process.h>
#include <kernel/sched/thread.h>

#include <assert.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/rcu.h>
#include <stdlib.h>

static cache_t cache = CACHE_CREATE(cache, "job", sizeof(job_t), CACHE_LINE, NULL, NULL);

void job_member_init(job_member_t* member)
{
    list_entry_init(&member->entry);
    member->job = NULL;
    lock_init(&member->lock);
}

void job_member_deinit(job_member_t* member)
{
    job_leave(member);
}

static void job_free(job_t* job)
{
    if (job == NULL)
    {
        return;
    }

    assert(list_is_empty(&job->members));
    assert(list_is_empty(&job->children));

    if (job->parent != NULL)
    {
        lock_acquire(&job->parent->lock);
        list_remove(&job->sibling);
        lock_release(&job->parent->lock);
        UNREF(job->parent);
    }

    cache_free(job);
}

status_t job_new(job_t** out, job_t* parent)
{
    if (out == NULL)
    {
        return ERR(PROC, INVAL);
    }

    job_t* job = cache_alloc(&cache);
    if (job == NULL)
    {
        return ERR(PROC, NOMEM);
    }
    ref_init(&job->ref, job_free);
    list_init(&job->members);
    lock_init(&job->lock);

    job->parent = NULL;
    list_init(&job->children);
    list_entry_init(&job->sibling);
    job->leader = NULL;

    if (parent != NULL)
    {
        job->parent = REF(parent);
        lock_acquire(&parent->lock);
        list_push_back(&parent->children, &job->sibling);
        lock_release(&parent->lock);
    }

    *out = job;
    return OK;
}

job_t* job_get(job_member_t* member)
{
    if (member == NULL)
    {
        return NULL;
    }

    LOCK_SCOPE(&member->lock);
    if (member->job == NULL)
    {
        return NULL;
    }

    return REF(member->job);
}

void job_join(job_t* job, job_member_t* member)
{
    if (job == NULL || member == NULL)
    {
        return;
    }

    LOCK_SCOPE(&member->lock);
    if (member->job != NULL)
    {
        lock_acquire(&member->job->lock);
        list_remove(&member->entry);
        if (member->job->leader == member)
        {
            member->job->leader = NULL;
        }
        lock_release(&member->job->lock);
        UNREF(member->job);
        member->job = NULL;
    }

    LOCK_SCOPE(&job->lock);
    list_push_back(&job->members, &member->entry);
    if (job->leader == NULL)
    {
        job->leader = member;
    }
    member->job = REF(job);
}

void job_leave(job_member_t* member)
{
    if (member == NULL)
    {
        return;
    }

    LOCK_SCOPE(&member->lock);

    if (member->job == NULL)
    {
        return;
    }

    lock_acquire(&member->job->lock);
    list_remove(&member->entry);
    if (member->job->leader == member)
    {
        member->job->leader = CONTAINER_OF_SAFE(list_first(&member->job->members), job_member_t, entry);
    }
    lock_release(&member->job->lock);

    UNREF(member->job);
    member->job = NULL;
}

bool job_is_accessible(job_t* job, job_t* target)
{
    if (job == NULL || target == NULL)
    {
        return false;
    }

    job_t* current = target;
    while (current != NULL)
    {
        if (current == job)
        {
            return true;
        }
        current = current->parent;
    }

    return false;
}