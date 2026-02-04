#include <kernel/mem/mdl.h>
#include <kernel/mem/paging_types.h>
#include <kernel/mem/space.h>
#include <kernel/proc/process.h>

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

status_t mdl_from_region(mdl_t* mdl, mdl_t* prev, space_t* space, const void* addr, size_t size)
{
    if (mdl == NULL)
    {
        return ERR(MMU, INVAL);
    }
    mdl_init(mdl, prev);

    status_t status = mdl_add(mdl, space, addr, size);
    if (IS_ERR(status))
    {
        mdl_deinit(mdl);
        return status;
    }

    return OK;
}

static status_t mdl_push(mdl_t* mdl, phys_addr_t phys, size_t size)
{
    if (size > UINT32_MAX)
    {
        return ERR(MMU, TOOBIG);
    }

    if (mdl == NULL)
    {
        return ERR(MMU, INVAL);
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
            return ERR(MMU, NOMEM);
        }

        mdl->segments = newSegments;
        mdl->capacity = newCapacity;
    }

    mdl_seg_t* seg = &mdl->segments[mdl->amount];
    pfn_t pfn = PHYS_TO_PFN(phys);
    uint32_t offset = phys % PAGE_SIZE;
    if (pmm_ref_inc(pfn, BYTES_TO_PAGES(offset + size)) == 0)
    {
        return ERR(MMU, FAULT);
    }

    seg->pfn = pfn;
    seg->size = size;
    seg->offset = offset;

    mdl->amount++;
    return OK;
}

status_t mdl_add(mdl_t* mdl, space_t* space, const void* addr, size_t size)
{
    const uint8_t* ptr = addr;
    size_t remaining = size;

    while (remaining > 0)
    {
        phys_addr_t phys;
        status_t status = space_virt_to_phys(&phys, space, ptr);
        if (IS_ERR(status))
        {
            return status;
        }

        size_t offset = phys % PAGE_SIZE;
        size_t len = MIN(remaining, PAGE_SIZE - offset);

        status = mdl_push(mdl, phys, len);
        if (IS_ERR(status))
        {
            return status;
        }

        ptr += len;
        remaining -= len;
    }

    return OK;
}

status_t mdl_copy_from_buffer(mdl_t* mdl, size_t count, size_t* offset, size_t* bytesCopied, const void* source,
    size_t sourceLength)
{
    if (source == NULL && sourceLength == 0)
    {
        if (bytesCopied != NULL)
        {
            *bytesCopied = 0;
        }
        return OK;
    }

    if (mdl == NULL || source == NULL || offset == NULL)
    {
        if (bytesCopied != NULL)
        {
            *bytesCopied = 0;
        }
        return ERR(MMU, INVAL);
    }

    size_t currentOffset = *offset;
    size_t start = 0;
    size_t i = 0;
    for (; i < mdl->amount; i++)
    {
        mdl_seg_t* seg = &mdl->segments[i];
        if (start + seg->size > currentOffset)
        {
            break;
        }
        start += seg->size;
    }

    const uint8_t* ptr = source;
    size_t remaining = MIN(sourceLength, count);

    size_t segOffset = currentOffset - start;
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

    if (bytesCopied != NULL)
    {
        *bytesCopied = count - remaining;
    }
    *offset += count - remaining;

    if (remaining > 0)
    {
        return INFO(MMU, MORE);
    }

    return OK;
}

status_t mdl_copy_to_buffer(mdl_t* mdl, size_t count, size_t* offset, size_t* bytesCopied, void* dest, size_t destLength)
{
    if (mdl == NULL || dest == NULL || offset == NULL)
    {
        if (bytesCopied != NULL)
        {
            *bytesCopied = 0;
        }
        return ERR(MMU, INVAL);
    }

    size_t currentOffset = *offset;
    size_t start = 0;
    size_t i = 0;
    for (; i < mdl->amount; i++)
    {
        mdl_seg_t* seg = &mdl->segments[i];
        if (start + seg->size > currentOffset)
        {
            break;
        }
        start += seg->size;
    }

    uint8_t* ptr = dest;
    size_t remaining = MIN(destLength, count);

    size_t segOffset = currentOffset - start;
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

    if (bytesCopied != NULL)
    {
        *bytesCopied = count - remaining;
    }
    *offset += count - remaining;

    return OK;
}

status_t mdl_copy_from_circular(mdl_t* mdl, size_t count, size_t* offset, size_t* bytesCopied, const void* src,
    size_t srcLen, size_t srcIndex)
{
    size_t index = srcIndex % srcLen;
    size_t chunk1 = MIN(count, srcLen - index);
    size_t chunk2 = count - chunk1;

    size_t bytesCopied1 = 0;
    size_t bytesCopied2 = 0;

    status_t status = mdl_copy_from_buffer(mdl, chunk1, offset, &bytesCopied1, (const uint8_t*)src + index, chunk1);
    if (IS_ERR(status))
    {
        return status;
    }

    if (chunk2 > 0)
    {
        status = mdl_copy_from_buffer(mdl, chunk2, offset, &bytesCopied2, src, chunk2);
    }

    if (bytesCopied != NULL)
    {
        *bytesCopied = bytesCopied1 + bytesCopied2;
    }

    return status;
}