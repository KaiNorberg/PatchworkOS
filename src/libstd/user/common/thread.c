#include "thread.h"
#include "syscalls.h"

#include <stdlib.h>

static _thread_t thread0;

static _Atomic(_thread_t*) threads[_THREADS_MAX];

static mtx_t entryMutex;

static uint64_t _thread_hash(tid_t id)
{
    return id % _THREADS_MAX;
}

static uint64_t _thread_insert(_thread_t* thread)
{
    uint64_t index = _thread_hash(thread->id);
    for (uint64_t i = 0; i < _THREADS_MAX; i++)
    {
        uint64_t probe = (index + i) % _THREADS_MAX;
        _thread_t* expected = NULL;
        if (atomic_compare_exchange_strong_explicit(&threads[probe], &expected, thread, memory_order_release,
                memory_order_relaxed))
        {
            return 0;
        }
    }
    return ERR;
}

static void _thread_remove(_thread_t* thread)
{
    uint64_t index = _thread_hash(thread->id);
    for (uint64_t i = 0; i < _THREADS_MAX; i++)
    {
        uint64_t probe = (index + i) % _THREADS_MAX;
        _thread_t* current = (_thread_t*)atomic_load_explicit(&threads[probe], memory_order_acquire);
        if (current == thread)
        {
            atomic_store_explicit(&threads[probe], NULL, memory_order_release);
            return;
        }
        if (current == NULL)
        {
            return;
        }
    }
}

static void _thread_init(_thread_t* thread)
{
    atomic_init(&thread->state, _THREAD_ATTACHED);
    thread->id = 0;
    thread->result = 0;
    thread->err = EOK;
    thread->func = NULL;
    thread->arg = NULL;
}

void _threading_init(void)
{
    for (uint64_t i = 0; i < _THREADS_MAX; i++)
    {
        atomic_init(&threads[i], NULL);
    }
    mtx_init(&entryMutex, mtx_recursive);

    _thread_init(&thread0);
    thread0.id = _syscall_gettid();

    _thread_insert(&thread0);
}

_THREAD_ENTRY_ATTRIBUTES static void _thread_entry(_thread_t* thread)
{
    // Synchronize with creator
    mtx_lock(&entryMutex);
    mtx_unlock(&entryMutex);

    thrd_exit(thread->func(thread->arg));
}

_thread_t* _thread_new(thrd_start_t func, void* arg)
{
    _thread_t* thread = malloc(sizeof(_thread_t));
    if (thread == NULL)
    {
        return NULL;
    }

    _thread_init(thread);
    thread->func = func;
    thread->arg = arg;

    mtx_lock(&entryMutex);

    thread->id = _syscall_thread_create(_thread_entry, thread);
    if (thread->id == ERR)
    {
        errno = _syscall_errno();
        mtx_unlock(&entryMutex);
        free(thread);
        return NULL;
    }

    if (_thread_insert(thread) == ERR)
    {
        errno = ENOSPC;
        mtx_unlock(&entryMutex);
        free(thread);
        return NULL;
    }
    mtx_unlock(&entryMutex);

    return thread;
}

void _thread_free(_thread_t* thread)
{
    _thread_remove(thread);
    if (thread != &thread0)
    {
        free(thread);
    }
}

_thread_t* _thread_get(tid_t id)
{
    uint64_t index = _thread_hash(id);
    for (uint64_t i = 0; i < _THREADS_MAX; i++)
    {
        uint64_t probe = (index + i) % _THREADS_MAX;
        _thread_t* thread = (_thread_t*)atomic_load_explicit(&threads[probe], memory_order_acquire);
        if (thread == NULL)
        {
            return NULL;
        }
        if (thread->id == id)
        {
            return thread;
        }
    }
    return NULL;
}
