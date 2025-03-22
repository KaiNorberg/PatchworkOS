#include "../platform/platform.h"
#if _PLATFORM_HAS_SCHEDULING

#include "thread.h"

static _Thread_t threads[_MAX_THRD];

void _ThreadingInit(void)
{
    atomic_init(&threads[0].ref, 1);
    atomic_init(&threads[0].running, true);
    threads[0].index = 0;
    threads[0].id = gettid();
    threads[0].result = 0;
    threads[0].err = 0;
}

_Thread_t* _ThreadReserve(void)
{
    for (uint64_t i = 0; i < _MAX_THRD; i++)
    {
        atomic_long expected = 0;
        if (atomic_compare_exchange_strong(&threads[i].ref, &expected, 1))
        {
            atomic_init(&threads[i].running, false);
            threads[i].index = i;
            threads[i].id = 0;
            threads[i].result = 0;
            threads[i].err = 0;
            return &threads[i];
        }
    }

    return NULL;
}

void _ThreadFree(_Thread_t* thread)
{
    atomic_store(&thread->running, false);
    atomic_store(&thread->ref, 0);
}

_Thread_t* _ThreadById(tid_t id)
{
    for (uint64_t i = 0; i < _MAX_THRD; i++)
    {
        if (threads[i].id == id && atomic_load(&threads[i].ref) != 0)
        {
            return _ThreadRef(&threads[i]);
        }
    }

    return NULL;
}

_Thread_t* _ThreadByIndex(uint64_t index)
{
    if (index >= _MAX_THRD)
    {
        return NULL;
    }

    if (atomic_load(&threads[index].ref) == 0)
    {
        return NULL;
    }

    return _ThreadRef(&threads[index]);
}

#endif
