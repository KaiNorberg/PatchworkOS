#include <kernel/mem/pool.h>

void pool_init(pool_t* pool, void* elements, size_t capacity, size_t elementSize, size_t nextOffset)
{
    atomic_init(&pool->used, 0);
    atomic_init(&pool->free, 0);
    pool->elements = elements;
    pool->elementSize = elementSize;
    pool->nextOffset = nextOffset;
    pool->capacity = capacity;

    for (size_t i = 0; i < capacity; i++)
    {
        void* element = (void*)((uintptr_t)elements + (i * elementSize));
        pool_idx_t* next = (pool_idx_t*)((uintptr_t)element + nextOffset);
        *next = (i == capacity - 1) ? POOL_IDX_MAX : (pool_idx_t)(i + 1);
    }
}

pool_idx_t pool_alloc(pool_t* pool)
{
    pool_idx_t idx;
    while (true)
    {
        uint64_t head = atomic_load_explicit(&pool->free, memory_order_acquire);
        idx = (pool_idx_t)(head & POOL_IDX_MAX);
        if (idx == POOL_IDX_MAX)
        {
            return POOL_IDX_MAX;
        }

        void* element = (void*)((uintptr_t)pool->elements + (idx * pool->elementSize));
        pool_idx_t next = *(pool_idx_t*)((uintptr_t)element + pool->nextOffset);

        uint64_t newHead = ((head & ~POOL_IDX_MAX) + POOL_TAG_INC) | next;
        if (atomic_compare_exchange_weak_explicit(&pool->free, &head, newHead, memory_order_acquire,
                memory_order_relaxed))
        {
            break;
        }
        ASM("pause");
    }

    atomic_fetch_add_explicit(&pool->used, 1, memory_order_relaxed);
    return idx;
}

void pool_free(pool_t* pool, pool_idx_t idx)
{
    void* element = (void*)((uintptr_t)pool->elements + (idx * pool->elementSize));
    pool_idx_t* next = (pool_idx_t*)((uintptr_t)element + pool->nextOffset);

    while (true)
    {
        uint64_t head = atomic_load_explicit(&pool->free, memory_order_relaxed);
        *next = (pool_idx_t)(head & POOL_IDX_MAX);
        uint64_t newHead = ((head & ~POOL_IDX_MAX) + POOL_TAG_INC) | idx;

        if (atomic_compare_exchange_weak_explicit(&pool->free, &head, newHead, memory_order_release,
                memory_order_relaxed))
        {
            break;
        }
        ASM("pause");
    }
    atomic_fetch_sub_explicit(&pool->used, 1, memory_order_relaxed);
}