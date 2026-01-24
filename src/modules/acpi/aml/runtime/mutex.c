#include <kernel/acpi/aml/runtime/mutex.h>

#include <kernel/log/log.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/thread.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/list.h>

typedef struct
{
    list_entry_t entry;
    aml_mutex_id_t id;
    aml_sync_level_t syncLevel;
} aml_mutex_entry_t;

static bool initialized = false;
static aml_sync_level_t currentSyncLevel = 0;
static list_t mutexStack;

static atomic_int32_t newMutexId = ATOMIC_VAR_INIT(1);

static inline uint64_t aml_mutex_stack_push(aml_mutex_id_t id, aml_sync_level_t syncLevel)
{
    if (!initialized)
    {
        list_init(&mutexStack);
        initialized = true;
    }

    if (syncLevel < currentSyncLevel)
    {
        LOG_ERR("Attempted to acquire a mutex with a lower SyncLevel than the current SyncLevel\n");
        errno = EDEADLK;
        return ERR;
    }

    aml_mutex_entry_t* entry = malloc(sizeof(aml_mutex_entry_t));
    if (entry == NULL)
    {
        return ERR;
    }
    list_entry_init(&entry->entry);
    entry->id = id;
    entry->syncLevel = syncLevel;

    list_push_back(&mutexStack, &entry->entry);
    currentSyncLevel = syncLevel;
    return 0;
}

static inline uint64_t aml_mutex_stack_pop(aml_mutex_id_t id)
{
    if (list_is_empty(&mutexStack))
    {
        LOG_ERR("Attempted to release a mutex when none are held\n");
        errno = EDEADLK;
        return ERR;
    }

    aml_mutex_entry_t* topEntry = CONTAINER_OF(list_last(&mutexStack), aml_mutex_entry_t, entry);
    if (topEntry->id != id)
    {
        LOG_ERR("Mutex release not in LIFO order\n");
        errno = EDEADLK;
        return ERR;
    }

    list_remove(&topEntry->entry);
    free(topEntry);

    if (list_is_empty(&mutexStack))
    {
        currentSyncLevel = 0;
    }
    else
    {
        aml_mutex_entry_t* newTopEntry = CONTAINER_OF(list_last(&mutexStack), aml_mutex_entry_t, entry);
        currentSyncLevel = newTopEntry->syncLevel;
    }
    return 0;
}

void aml_mutex_id_init(aml_mutex_id_t* mutex)
{
    *mutex = atomic_fetch_add(&newMutexId, 1);
}

void aml_mutex_id_deinit(aml_mutex_id_t* mutex)
{
    *mutex = 0;
}

uint64_t aml_mutex_acquire(aml_mutex_id_t* mutex, aml_sync_level_t syncLevel, clock_t timeout)
{
    UNUSED(timeout); // We ignore timeouts since we have the big mutex.

    if (mutex == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    // As mentioned, mutexes arent implemented since we have the big mutex, so we just pretend that
    // we acquired it immediately.

    if (aml_mutex_stack_push(*mutex, syncLevel) == ERR)
    {
        return ERR;
    }
    return 0;
}

uint64_t aml_mutex_release(aml_mutex_id_t* mutex)
{
    if (mutex == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (aml_mutex_stack_pop(*mutex) == ERR)
    {
        return ERR;
    }
    return 0;
}
