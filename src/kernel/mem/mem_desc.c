#include <kernel/mem/mem_desc.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/space.h>
#include <kernel/proc/process.h>

#include <errno.h>
#include <stdlib.h>
#include <sys/proc.h>

static inline mem_seg_t* mem_desc_get_seg(mem_desc_t* desc, size_t index)
{
    if (index >= desc->amount)
    {
        return NULL;
    }

    if (index < MEM_SEGS_SMALL_MAX)
    {
        return &desc->small[index];
    }
    
    return &desc->large[index - MEM_SEGS_SMALL_MAX];
}

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

mem_desc_t* mem_desc_new(mem_desc_pool_t* pool)
{
    if (pool == NULL)
    {
        errno = EINVAL;
        return NULL;
    }

    pool_idx_t idx = pool_alloc(&pool->pool);
    if (idx == POOL_IDX_MAX)
    {
        errno = ENOSPC;
        return NULL;
    }

    mem_desc_t* desc = &pool->descs[idx];
    desc->next = POOL_IDX_MAX;
    desc->amount = 0;
    desc->capacity = 0;
    desc->size = 0;
    desc->large = NULL;
    return desc;
}

void mem_desc_free(mem_desc_t* desc)
{
    if (desc == NULL)
    {
        return;
    }

    size_t i = 0;
    for (; i < desc->amount && i < MEM_SEGS_SMALL_MAX; i++)
    {
        pmm_ref_dec(desc->small[i].pfn, BYTES_TO_PAGES(desc->small[i].offset + desc->small[i].size));
    }

    for (; i < desc->amount; i++)
    {
        pmm_ref_dec(desc->large[i - MEM_SEGS_SMALL_MAX].pfn,
            BYTES_TO_PAGES(desc->large[i - MEM_SEGS_SMALL_MAX].offset + desc->large[i - MEM_SEGS_SMALL_MAX].size));
    }

    if (desc->large != NULL)
    {
        free(desc->large);
    }
    desc->large = NULL;

    mem_desc_pool_t* pool = mem_desc_pool_get(desc);
    pool_free(&pool->pool, desc->index);
}

uint64_t mem_desc_add(mem_desc_t* desc, phys_addr_t phys, size_t size)
{
    if (desc == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

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
        mem_seg_t* newLarge = realloc(desc->large, (desc->capacity + 4) * sizeof(mem_seg_t));
        if (newLarge == NULL)
        {
            errno = ENOMEM;
            return ERR;
        }

        desc->large = newLarge;
        desc->capacity += 4;
        seg = &desc->large[desc->amount++ - MEM_SEGS_SMALL_MAX];
    }

    pfn_t pfn = PHYS_TO_PFN(phys);
    uint32_t offset = phys % PAGE_SIZE;
    if (pmm_ref_inc(pfn, BYTES_TO_PAGES(offset + size)) == ERR)
    {
        errno = EFAULT;
        return ERR;
    }

    seg->pfn = pfn;
    seg->size = size;
    seg->offset = offset;
    desc->size += size;
    return 0;
}

uint64_t mem_desc_add_user(mem_desc_t* desc, process_t* process, const void* addr, size_t size)
{
    const uint8_t* ptr = addr;
    size_t remaining = size;

    while (remaining > 0)
    {
        phys_addr_t phys = space_virt_to_phys(&process->space, ptr);
        if (phys == ERR)
        {
            return ERR;
        }

        size_t offset = phys % PAGE_SIZE;
        size_t len = MIN(remaining, PAGE_SIZE - offset);

        if (mem_desc_add(desc, phys, len) == ERR)
        {
            return ERR;
        }

        ptr += len;
        remaining -= len;
    }

    return 0;
}

uint64_t mem_desc_read(mem_desc_t* desc, void* buffer, size_t count, size_t offset)
{
    if (desc == NULL || buffer == NULL || offset + count > desc->size)
    {
        return 0;
    }

}

uint64_t mem_desc_write(mem_desc_t* desc, const void* buffer, size_t count, size_t offset)
{
    if (desc == NULL || buffer == NULL || offset + count > desc->size)
    {
        return 0;
    }
}

 uint64_t mem_desc_copy(mem_desc_t* dest, size_t destOffset, mem_desc_t* src, size_t srcOffset,
    size_t count)
{
    if (dest == NULL || src == NULL || destOffset + count > dest->size || srcOffset + count > src->size)
    {
        return 0;
    }
}