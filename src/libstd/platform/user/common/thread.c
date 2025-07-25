#include "thread.h"
#include "syscalls.h"

#include <stdlib.h>

static _thread_t thread0;

static list_t threads;
static mtx_t mutex;

void _threading_init(void)
{
    list_init(&threads);
    mtx_init(&mutex, mtx_recursive);

    // We cant yet use the heap, so we do this weird stuff
    list_entry_init(&thread0.entry);
    atomic_init(&thread0.state, _THREAD_ATTACHED);
    thread0.id = gettid();
    thread0.result = 0;
    thread0.err = 0;
    thread0.private = NULL;

    list_push(&threads, &thread0.entry);
}

_thread_t* _thread_new(_thread_entry_t entry, void* private)
{
    _thread_t* thread = malloc(sizeof(_thread_t));
    if (thread == NULL)
    {
        return NULL;
    }

    mtx_lock(&mutex);

    list_entry_init(&thread->entry);
    atomic_init(&thread->state, _THREAD_ATTACHED);
    thread->result = 0;
    thread->err = 0;
    thread->private = private;

    thread->id = _syscall_thread_create(entry, thread);
    if (thread->id == ERR)
    {
        errno = _syscall_errno();

        mtx_unlock(&mutex);
        free(thread);
        return NULL;
    }

    list_push(&threads, &thread->entry);
    mtx_unlock(&mutex);

    return thread;
}

void _thread_free(_thread_t* thread)
{
    mtx_lock(&mutex);
    list_remove(&thread->entry);
    mtx_unlock(&mutex);
    if (thread != &thread0)
    {
        free(thread);
    }
}

_thread_t* _thread_get(tid_t id)
{
    mtx_lock(&mutex);
    _thread_t* thread;
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
