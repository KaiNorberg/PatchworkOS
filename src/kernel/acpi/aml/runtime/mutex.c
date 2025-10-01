#include "mutex.h"

#include "mem/heap.h"
#include "log/log.h"

#include <errno.h>

uint64_t aml_mutex_stack_init(aml_mutex_stack_t* mutexStack)
{
    if (mutexStack == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    mutexStack->acquiredMutexes = NULL;
    mutexStack->acquiredMutexCount = 0;
    mutexStack->acquiredMutexCapacity = 0;
    mutexStack->currentSyncLevel = 0;

    return 0;
}

void aml_mutex_stack_deinit(aml_mutex_stack_t* mutexStack)
{
    if (mutexStack == NULL)
    {
        return;
    }

    for (uint64_t i = 0; i < mutexStack->acquiredMutexCount; i++)
    {
        mutex_release(&mutexStack->acquiredMutexes[i].mutex->mutex.mutex);
    }

    heap_free(mutexStack->acquiredMutexes);
    mutexStack->acquiredMutexes = NULL;
    mutexStack->acquiredMutexCount = 0;
    mutexStack->acquiredMutexCapacity = 0;
    mutexStack->currentSyncLevel = 0;
}

static inline uint64_t aml_mutex_stack_push_mutex(aml_mutex_stack_t* mutexStack, aml_object_t* mutex)
{
    if (mutexStack->acquiredMutexCount >= mutexStack->acquiredMutexCapacity)
    {
        uint64_t newCapacity = mutexStack->acquiredMutexCapacity == 0 ? 4 : mutexStack->acquiredMutexCapacity * 2;
        aml_mutex_stack_entry_t* newStack = heap_realloc(mutexStack->acquiredMutexes,
            sizeof(aml_mutex_stack_entry_t) * newCapacity, HEAP_NONE);
        if (newStack == NULL)
        {
            return ERR;
        }
        mutexStack->acquiredMutexes = newStack;
        mutexStack->acquiredMutexCapacity = newCapacity;
    }

    mutexStack->acquiredMutexes[mutexStack->acquiredMutexCount].mutex = mutex;
    mutexStack->acquiredMutexCount++;
    mutexStack->currentSyncLevel = mutex->mutex.syncLevel;

    return 0;
}

static inline void aml_mutex_stack_pop_mutex(aml_mutex_stack_t* mutexStack)
{
    if (mutexStack->acquiredMutexCount == 0)
    {
        return;
    }

    mutexStack->acquiredMutexCount--;
    if (mutexStack->acquiredMutexCount == 0)
    {
        heap_free(mutexStack->acquiredMutexes);
        mutexStack->acquiredMutexes = NULL;
        mutexStack->currentSyncLevel = 0;
    }
    else
    {
        mutexStack->currentSyncLevel = mutexStack->acquiredMutexes[mutexStack->acquiredMutexCount - 1].mutex->mutex.syncLevel;
        aml_mutex_stack_entry_t* newStack = heap_realloc(mutexStack->acquiredMutexes,
            sizeof(aml_mutex_stack_entry_t) * mutexStack->acquiredMutexCount, HEAP_NONE);
        if (newStack != NULL)
        {
            mutexStack->acquiredMutexes = newStack;
        }
    }
}

static inline aml_mutex_stack_entry_t* aml_mutex_stack_find_acquired_mutex(aml_mutex_stack_t* mutexStack, aml_object_t* mutex)
{
    for (uint64_t i = 0; i < mutexStack->acquiredMutexCount; i++)
    {
        if (mutexStack->acquiredMutexes[i].mutex == mutex)
        {
            return &mutexStack->acquiredMutexes[i];
        }
    }
    return NULL;
}

uint64_t aml_mutex_stack_acquire(aml_mutex_stack_t* mutexStack, aml_object_t* mutex, clock_t timeout)
{
    if (mutex->type != AML_DATA_MUTEX)
    {
        LOG_ERR("Object is not a mutex\n");
        errno = EINVAL;
        return ERR;
    }

    if (mutex->mutex.syncLevel < mutexStack->currentSyncLevel)
    {
        LOG_ERR("Cannot acquire mutex with lower sync level than current level (%u < %u)\n",
            mutex->mutex.syncLevel, mutexStack->currentSyncLevel);
        errno = EDEADLK;
        return ERR;
    }

    if (aml_mutex_stack_find_acquired_mutex(mutexStack, mutex) != NULL)
    {
        return 0;
    }

    if (!mutex_acquire_timeout(&mutex->mutex.mutex, timeout))
    {
        return 1;
    }

    if (aml_mutex_stack_push_mutex(mutexStack, mutex) == ERR)
    {
        mutex_release(&mutex->mutex.mutex);
        return ERR;
    }

    return 0;
}

uint64_t aml_mutex_stack_release(aml_mutex_stack_t* mutexStack, aml_object_t* mutex)
{
    if (mutex->type != AML_DATA_MUTEX)
    {
        LOG_ERR("Object is not a mutex\n");
        errno = EINVAL;
        return ERR;
    }

    if (mutexStack->acquiredMutexCount == 0)
    {
        LOG_ERR("Attempted to release a mutex when none are held\n");
        errno = EDEADLK;
        return ERR;
    }

    aml_object_t* topMutex = mutexStack->acquiredMutexes[mutexStack->acquiredMutexCount - 1].mutex;
    if (topMutex != mutex)
    {
        LOG_ERR("Mutex release not in LIFO order\n");
        errno = EDEADLK;
        return ERR;
    }

    mutex_release(&mutex->mutex.mutex);

    mutexStack->acquiredMutexCount--;
    if (mutexStack->acquiredMutexCount == 0)
    {
        mutexStack->currentSyncLevel = 0;
    }
    else
    {
        mutexStack->currentSyncLevel = mutexStack->acquiredMutexes[mutexStack->acquiredMutexCount - 1].mutex->mutex.syncLevel;
    }

    return 0;
}
