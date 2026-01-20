#include <kernel/mem/mem_desc.h>

#include <stdlib.h>
#include <errno.h>

mem_desc_pool_t* mem_desc_pool_new(size_t size)
{
    size_t poolSize = sizeof(mem_desc_pool_t) + (size * sizeof(mem_desc_t));
    mem_desc_pool_t* pool = malloc(poolSize);
    if (pool == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    for (size_t i = 0; i < size; i++)
    {
        pool->descs[i].index = i;
    }
    pool_init(&pool->pool, pool->descs, size, sizeof(mem_desc_t), offsetof(mem_desc_t, next));
    return pool;
}

void mem_desc_pool_free(mem_desc_pool_t* pool)
{
    free(pool);
}

uint64_t mem_desc_add(mem_desc_t* desc, void* addr, size_t size)
{
    mem_seg_t* seg = NULL;
    if (desc->amount < MEM_SEGS_SMALL_MAX)
    {
        seg = &desc->small[desc->amount++];
    }
    else if (desc->amount - MEM_SEGS_SMALL_MAX < desc->capacity)
    {
        seg = &desc->large[desc->amount++ - MEM_SEGS_SMALL_MAX];
    }
    else
    {
        mem_seg_t* newLarge = realloc(
            desc->large, (desc->capacity + 4) * sizeof(mem_seg_t));
        if (newLarge == NULL)
        {
            return ERR;
        }

        desc->large = newLarge;
        desc->capacity += 4;
        seg = &desc->large[desc->amount++ - MEM_SEGS_SMALL_MAX];
    }

    seg->page = (void*)ROUND_DOWN(addr, PAGE_SIZE);
    seg->offset = (uint32_t)((uintptr_t)addr - (uintptr_t)seg->page);
    seg->length = (uint32_t)size;
    return 0;
}