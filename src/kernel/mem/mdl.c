#include <kernel/mem/mdl.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/space.h>
#include <kernel/proc/process.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/proc.h>

void mdl_deinit(mdl_t* mdl)
{
    if (mdl == NULL)
    {
        return;
    }

    mdl->next = NULL;

    for (size_t i = 0; i < mdl->amount; i++)
    {
        pmm_ref_dec(mdl->segments[i].pfn, BYTES_TO_PAGES(mdl->segments[i].offset + mdl->segments[i].size));
    }
    mdl->amount = 0;

    if (mdl->segments != mdl->small)
    {
        free(mdl->segments);
    }
    mdl->segments = NULL;
    mdl->capacity = 0;
}

void mdl_free_chain(mdl_t* mdl, void (*free)(void*))
{
    while (mdl != NULL)
    {
        mdl_t* next = mdl->next;
        mdl_deinit(mdl);
        if (free != NULL)
        {
            free(mdl);
        }
        mdl = next;
    }
}

uint64_t mdl_from_region(mdl_t* mdl, mdl_t* prev, space_t* space, const void* addr, size_t size)
{
    if (mdl == NULL)
    {
        errno = EINVAL;
        return ERR;
    }
    mdl_init(mdl, prev);

    if (mdl_add(mdl, space, addr, size) == ERR)
    {
        mdl_deinit(mdl);
        return ERR;
    }

    return 0;
}

static uint64_t mdl_push(mdl_t* mdl, phys_addr_t phys, size_t size)
{
    if (size > UINT32_MAX)
    {
        errno = EOVERFLOW;
        return ERR;
    }

    if (mdl == NULL)
    {
        errno = EINVAL;
        return ERR;
    }

    if (mdl->amount == mdl->capacity)
    {
        uint32_t newCapacity = mdl->capacity + 4;
        mdl_seg_t* newSegments;

        if (mdl->segments == mdl->small)
        {
            newSegments = malloc(newCapacity * sizeof(mdl_seg_t));
            if (newSegments != NULL)
            {
                memcpy(newSegments, mdl->small, sizeof(mdl->small));
            }
        }
        else
        {
            newSegments = realloc(mdl->segments, newCapacity * sizeof(mdl_seg_t));
        }

        if (newSegments == NULL)
        {
            errno = ENOMEM;
            return ERR;
        }

        mdl->segments = newSegments;
        mdl->capacity = newCapacity;
    }

    mdl_seg_t* seg = &mdl->segments[mdl->amount];
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

    mdl->amount++;
    return 0;
}

uint64_t mdl_add(mdl_t* mdl, space_t* space, const void* addr, size_t size)
{
    const uint8_t* ptr = addr;
    size_t remaining = size;

    while (remaining > 0)
    {
        phys_addr_t phys = space_virt_to_phys(space, ptr);
        if (phys == ERR)
        {
            return ERR;
        }

        size_t offset = phys % PAGE_SIZE;
        size_t len = MIN(remaining, PAGE_SIZE - offset);

        if (mdl_push(mdl, phys, len) == ERR)
        {
            return ERR;
        }

        ptr += len;
        remaining -= len;
    }

    return 0;
}

uint64_t mdl_read(mdl_t* mdl, void* buffer, size_t count, size_t offset)
{
    if (mdl == NULL || buffer == NULL)
    {
        return 0;
    }

    size_t start = 0;
    size_t i = 0;
    for (; i < mdl->amount; i++)
    {
        mdl_seg_t* seg = &mdl->segments[i];
        if (start + seg->size > offset)
        {
            break;
        }
        start += seg->size;
    }

    uint8_t* ptr = buffer;
    size_t remaining = count;

    size_t segOffset = offset - start;
    while (remaining > 0 && i < mdl->amount)
    {
        mdl_seg_t* seg = &mdl->segments[i];
        size_t toRead = MIN(remaining, seg->size - segOffset);
        void* addr = PFN_TO_VIRT(seg->pfn) + seg->offset + segOffset;
        memcpy(ptr, addr, toRead);

        ptr += toRead;
        remaining -= toRead;
        segOffset = 0;
        i++;
    }

    return count - remaining;
}

uint64_t mdl_write(mdl_t* mdl, const void* buffer, size_t count, size_t offset)
{
    if (mdl == NULL || buffer == NULL)
    {
        return 0;
    }

    size_t start = 0;
    size_t i = 0;
    for (; i < mdl->amount; i++)
    {
        mdl_seg_t* seg = &mdl->segments[i];
        if (start + seg->size > offset)
        {
            break;
        }
        start += seg->size;
    }

    const uint8_t* ptr = buffer;
    size_t remaining = count;

    size_t segOffset = offset - start;
    while (remaining > 0 && i < mdl->amount)
    {
        mdl_seg_t* seg = &mdl->segments[i];
        size_t toWrite = MIN(remaining, seg->size - segOffset);
        void* addr = PFN_TO_VIRT(seg->pfn) + seg->offset + segOffset;
        memcpy(addr, ptr, toWrite);

        ptr += toWrite;
        remaining -= toWrite;
        segOffset = 0;
        i++;
    }

    return count - remaining;
}