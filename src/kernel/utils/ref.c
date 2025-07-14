#include "ref.h"

#include <assert.h>
#include <stdatomic.h>

void ref_init(ref_t* ref, void* free)
{
#ifndef NDEBUG
    ref->magic = REF_MAGIC;
#endif
    atomic_init(&ref->count, 1);
    ref->free = free;
}

void* ref_inc(void* ptr)
{
    ref_t* ref = (ref_t*)ptr;
    if (ref == NULL)
    {
        return NULL;
    }

    assert(ref->magic == REF_MAGIC);
    atomic_fetch_add_explicit(&ref->count, 1, memory_order_relaxed);
    return ptr;
}

void ref_dec(void* ptr)
{
    ref_t* ref = (ref_t*)ptr;
    if (ref == NULL)
    {
        return;
    }

    assert(ref->magic == REF_MAGIC);
    uint64_t count = atomic_fetch_sub_explicit(&ref->count, 1, memory_order_relaxed);
    if (count > 1)
    {
        return;
    }

    atomic_thread_fence(memory_order_acquire);
    assert(count == 1); // Count is now zero, if it was zero before then we have a double free.
    if (ref->free == NULL)
    {
        return;
    }

    ref->free(ptr);
}
