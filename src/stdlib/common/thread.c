#include "../platform/platform.h"
#if _PLATFORM_HAS_SCHEDULING

#include "thread.h"

#include <stdlib.h>

static _Thread_t thread0;

static list_t threads;
static mtx_t mutex;

void _ThreadingInit(void)
{
    list_init(&threads);
    mtx_init(&mutex, mtx_plain);

    // We cant yet use the heap, so we do this weird stuff
    list_entry_init(&thread0.entry);
    atomic_init(&thread0.running, true);
    atomic_init(&thread0.ref, 1);
    thread0.id = gettid();
    thread0.result = 0;
    thread0.err = 0;

    list_push(&threads, &thread0.entry);
}

_Thread_t* _ThreadNew(void)
{
    _Thread_t* thread = malloc(sizeof(_Thread_t));
    if (thread == NULL)
    {
        return NULL;
    }

    list_entry_init(&thread->entry);
    atomic_init(&thread->running, false);
    atomic_init(&thread->ref, 1);
    thread->id = 0;
    thread->result = 0;
    thread->err = 0;

    mtx_lock(&mutex);
    list_push(&threads, &thread->entry);
    mtx_unlock(&mutex);

    return thread;
}

void _ThreadFree(_Thread_t* thread)
{
    mtx_lock(&mutex);
    list_remove(&thread->entry);
    if (thread != &thread0)
    {
        free(thread);
    }
    mtx_unlock(&mutex);
}

_Thread_t* _ThreadById(tid_t id)
{
    mtx_lock(&mutex);

    _Thread_t* thread;
    LIST_FOR_EACH(thread, &threads, entry)
    {
        if (thread->id == id)
        {
            mtx_unlock(&mutex);
            return thread;
        }
    }

    mtx_unlock(&mutex);
    return NULL;
}

#endif
