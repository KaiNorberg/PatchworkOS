#include <kernel/mem/pagevec.h>

#include <kernel/mem/pmm.h>

#include <libstd/math.h>
#include <stdlib.h>
#include <string.h>

void pagevec_deinit(pagevec_t* vec)
{
    if (vec == NULL || vec->pfns == NULL)
    {
        return;
    }

    for (size_t i = 0; i < vec->amount; i++)
    {
        pmm_free(vec->pfns[i]);
    }

    free(vec->pfns);
    vec->pfns = NULL;
    vec->amount = 0;
    vec->capacity = 0;
}

status_t pagevec_resize(pagevec_t* vec, size_t pageAmount)
{
    if (vec == NULL)
    {
        return ERR(MMU, INVAL);
    }

    if (pageAmount > vec->amount)
    {
        if (pageAmount > vec->capacity)
        {
            size_t newCapacity = MAX(vec->capacity * 2, pageAmount);
            pfn_t* newPfns = realloc(vec->pfns, newCapacity * sizeof(pfn_t));
            if (newPfns == NULL)
            {
                return ERR(MMU, NOMEM);
            }
            vec->pfns = newPfns;
            vec->capacity = newCapacity;
        }

        for (size_t i = vec->amount; i < pageAmount; i++)
        {
            vec->pfns[i] = pmm_alloc();
            if (vec->pfns[i] == PFN_INVALID)
            {
                vec->amount = i;
                return ERR(MMU, NOMEM);
            }
            memset(PFN_TO_VIRT(vec->pfns[i]), 0, PAGE_SIZE);
        }
        vec->amount = pageAmount;
    }
    else if (pageAmount < vec->amount)
    {
        for (size_t i = pageAmount; i < vec->amount; i++)
        {
            pmm_free(vec->pfns[i]);
        }
        vec->amount = pageAmount;
    }

    return OK;
}

void pagevec_write(pagevec_t* vec, size_t offset, const void* buffer, size_t length)
{
    const uint8_t* p = buffer;
    size_t remaining = length;
    while (remaining > 0)
    {
        size_t pageIdx = offset / PAGE_SIZE;
        size_t pageOff = offset % PAGE_SIZE;
        size_t chunk = MIN(remaining, PAGE_SIZE - pageOff);

        memcpy(PFN_TO_VIRT(vec->pfns[pageIdx]) + pageOff, p, chunk);

        p += chunk;
        offset += chunk;
        remaining -= chunk;
    }
}

void pagevec_read(const pagevec_t* vec, size_t offset, void* buffer, size_t length)
{
    uint8_t* p = buffer;
    size_t remaining = length;
    while (remaining > 0)
    {
        size_t pageIdx = offset / PAGE_SIZE;
        size_t pageOff = offset % PAGE_SIZE;
        size_t chunk = MIN(remaining, PAGE_SIZE - pageOff);

        memcpy(p, PFN_TO_VIRT(vec->pfns[pageIdx]) + pageOff, chunk);

        p += chunk;
        offset += chunk;
        remaining -= chunk;
    }
}

void pagevec_fill(pagevec_t* vec, size_t offset, uint8_t value, size_t length)
{
    size_t remaining = length;
    while (remaining > 0)
    {
        size_t pageIdx = offset / PAGE_SIZE;
        size_t pageOff = offset % PAGE_SIZE;
        size_t chunk = MIN(remaining, PAGE_SIZE - pageOff);

        memset(PFN_TO_VIRT(vec->pfns[pageIdx]) + pageOff, value, chunk);

        offset += chunk;
        remaining -= chunk;
    }
}