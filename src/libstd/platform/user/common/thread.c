#include "thread.h"
#include "syscalls.h"

#include <stdlib.h>

static _thread_t thread0;

static list_t threads;
static mtx_t mutex;

static void _thread_init(_thread_t* thread)
{
    list_entry_init(&thread->entry);
    atomic_init(&thread->state, _THREAD_ATTACHED);
    thread->id = 0;
    thread->result = 0;
    thread->err = EOK;
    thread->private = NULL;
}

void _threading_init(void)
{
    list_init(&threads);
    mtx_init(&mutex, mtx_recursive);

    // We cant yet use the heap yet
    _thread_init(&thread0);
    thread0.id = _syscall_gettid();

    list_push(&threads, &thread0.entry);
}

_thread_t* _thread_new(_thread_entry_t entry, void* private)
{
    _thread_t* thread = malloc(sizeof(_thread_t));
    if (thread == NULL)
    {
        return NULL;
    }

    _thread_init(thread);
    thread->private = private;

    mtx_lock(&mutex);
    list_push(&threads, &thread->entry);
    mtx_unlock(&mutex);

    thread->id = _syscall_thread_create(entry, thread);
    if (thread->id == ERR)
    {
        errno = _syscall_errno();

        mtx_lock(&mutex);
        list_remove(&threads, &thread->entry);
        mtx_unlock(&mutex);

        free(thread);
        return NULL;
    }

    return thread;
}

void _thread_free(_thread_t* thread)
{
    mtx_lock(&mutex);
    list_remove(&threads, &thread->entry);
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
