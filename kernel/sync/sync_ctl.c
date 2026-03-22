#include <kernel/cpu/syscall.h>
#include <kernel/log/log.h>
#include <kernel/mem/space.h>
#include <kernel/proc/process.h>
#include <kernel/sched/clock.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>
#include <kernel/sched/timer.h>
#include <kernel/sched/wait.h>
#include <kernel/sync/lock.h>
#include <kernel/sync/seqlock.h>
#include <kernel/sync/sync_ctl.h>
#include <kernel/utils/ref.h>

#include <stdlib.h>
#include <sys/map.h>
#include <sys/sync.h>

typedef struct
{
    ref_t ref;
    map_entry_t entry;
    wait_queue_t queue;
    phys_addr_t addr;
} sync_object_t;

static cache_t cache = CACHE_CREATE(cache, "sync", sizeof(sync_object_t), CACHE_LINE, NULL, NULL);

static bool sync_map_cmp(map_entry_t* entry, const void* key)
{
    sync_object_t* object = CONTAINER_OF(entry, sync_object_t, entry);
    return object->addr == (phys_addr_t)key;
}

static MAP_CREATE(syncMap, 256, sync_map_cmp);
static seqlock_t syncLock = SEQLOCK_CREATE();

static void sync_object_free(void* ptr)
{
    sync_object_t* object = (sync_object_t*)ptr;

    seqlock_write_acquire(&syncLock);
    map_remove(&syncMap, &object->entry, hash_uint64(object->addr));
    seqlock_write_release(&syncLock);

    wait_queue_deinit(&object->queue);
    cache_free(object);
}

static sync_object_t* sync_object_get(phys_addr_t addr)
{
    seqlock_write_acquire(&syncLock);

    uint64_t hash = hash_uint64(addr);
    sync_object_t* object = CONTAINER_OF_SAFE(map_find(&syncMap, (void*)addr, hash), sync_object_t, entry);
    if (object != NULL)
    {
        if (REF_TRY(object) == NULL)
        {
            map_remove(&syncMap, &object->entry, hash);
            object = NULL;
        }
    }

    if (object == NULL)
    {
        object = cache_alloc(&cache);
        if (object == NULL)
        {
            seqlock_write_release(&syncLock);
            return NULL;
        }
        ref_init(&object->ref, sync_object_free);
        map_entry_init(&object->entry);
        wait_queue_init(&object->queue);
        object->addr = addr;

        map_insert(&syncMap, &object->entry, hash);
    }

    seqlock_write_release(&syncLock);
    return object;
}

static sync_object_t* sync_object_lookup(phys_addr_t addr)
{
    seqlock_write_acquire(&syncLock);

    uint64_t hash = hash_uint64(addr);
    sync_object_t* object = CONTAINER_OF_SAFE(map_find(&syncMap, (void*)addr, hash), sync_object_t, entry);
    if (object != NULL)
    {
        if (REF_TRY(object) == NULL)
        {
            object = NULL;
        }
    }

    seqlock_write_release(&syncLock);
    return object;
}

SYSCALL_DEFINE(SYS_SYNC_CTL, atomic_uint64_t* addr, uint64_t val, sync_op_t op, clock_t timeout)
{
    thread_t* thread = thread_current();
    process_t* process = thread->process;

    phys_addr_t phys;
    status_t status = space_virt_to_phys(&process->space, addr, &phys);
    if (IS_ERR(status))
    {
        return status;
    }

    switch (op)
    {
    case SYNC_WAIT:
    {
        sync_object_t* object = sync_object_get(phys);
        if (object == NULL)
        {
            return ERR(SYNC, NOMEM);
        }
        UNREF_DEFER(object);

        status = wait_block_prepare(&object->queue, timeout);
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
        return status;
    }
    case SYNC_WAKE:
    {
        sync_object_t* object = sync_object_lookup(phys);
        if (object == NULL)
        {
            return 0;
        }
        UNREF_DEFER(object);

        return wait_unblock(&object->queue, val, OK);
    }
    default:
    {
        return ERR(SYNC, INVAL);
    }
    }
}
