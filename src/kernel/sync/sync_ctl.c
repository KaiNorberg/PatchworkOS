#include <kernel/cpu/syscall.h>
#include <kernel/log/log.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/sync_ctl.h>

#include <stdlib.h>
#include <sys/map.h>
#include <sys/sync.h>

static bool sync_ctl_cmp(map_entry_t* entry, const void* key)
{
    sync_object_t* object = CONTAINER_OF(entry, sync_object_t, entry);
    return object->addr == (uintptr_t)key;
}

void sync_ctl_init(sync_ctl_t* ctl)
{
    MAP_DEFINE_INIT(ctl->objects, sync_ctl_cmp);
    lock_init(&ctl->lock);
}

void sync_ctl_deinit(sync_ctl_t* ctl)
{
    LOCK_SCOPE(&ctl->lock);

    sync_object_t* object;
    sync_object_t* temp;
    MAP_FOR_EACH_SAFE(object, temp, &ctl->objects, entry)
    {
        wait_queue_deinit(&object->queue);
        free(object);
    }
}

static sync_object_t* sync_ctl_get(sync_ctl_t* ctl, void* addr)
{
    LOCK_SCOPE(&ctl->lock);

    uint64_t hash = hash_uint64((uintptr_t)addr);
    sync_object_t* object = CONTAINER_OF_SAFE(map_find(&ctl->objects, addr, hash), sync_object_t, entry);
    if (object == NULL)
    {
        return NULL;
    }

    object = malloc(sizeof(sync_object_t));
    if (object == NULL)
    {
        return NULL;
    }
    map_entry_init(&object->entry);
    wait_queue_init(&object->queue);

    map_insert(&ctl->objects, &object->entry, hash);
    return object;
}

SYSCALL_DEFINE(SYS_SYNC_CTL, atomic_uint64_t* addr, uint64_t val, sync_op_t op, clock_t timeout)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;
    sync_ctl_t* ctl = &process->sync;

    sync_object_t* object = sync_ctl_get(ctl, addr);
    if (object == NULL)
    {
        return ERR(SYNC, NOMEM);
    }

    switch (op)
    {
    case SYNC_WAIT:
    {
        wait_queue_t* queue = &object->queue;

        status_t status = wait_block_prepare(&queue, 1, timeout);
        if (IS_ERR(status))
        {
            return status;
        }

        uint64_t loadedVal;
        status = space_copy_out(&process->space, &loadedVal, addr, sizeof(uint64_t));
        if (IS_ERR(status))
        {
            wait_block_cancel();
            return status;
        }

        if (loadedVal != val)
        {
            wait_block_cancel();
            return ERR(SYNC, CHANGED);
        }

        status = wait_block_commit();
        if (IS_ERR(status))
        {
            return status;
        }

        return OK;
    }
    case SYNC_WAKE:
    {
        return wait_unblock(&object->queue, val, OK);
    }
    default:
    {
        return ERR(SYNC, INVAL);
    }
    }
}
