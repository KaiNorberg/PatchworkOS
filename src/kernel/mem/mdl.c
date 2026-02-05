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
        pmm_ref_dec(mdl->descs[i].pfn, BYTES_TO_PAGES(mdl->descs[i].offset + mdl->descs[i].size));
    }
    mdl->amount = 0;

    if (mdl->descs != mdl->small)
    {
        free(mdl->descs);
    }
    mdl->descs = NULL;
    mdl->capacity = 0;
    mdl->size = 0;
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
        mdl_desc_t* newDescs;

        if (mdl->descs == mdl->small)
        {
            newDescs = malloc(newCapacity * sizeof(mdl_desc_t));
            if (newDescs != NULL)
            {
                memcpy(newDescs, mdl->small, sizeof(mdl->small));
            }
        }
        else
        {
            newDescs = realloc(mdl->descs, newCapacity * sizeof(mdl_desc_t));
        }

        if (newDescs == NULL)
        {
            return ERR(MMU, NOMEM);
        }

        mdl->descs = newDescs;
        mdl->capacity = newCapacity;
    }

    mdl_desc_t* seg = &mdl->descs[mdl->amount];
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
    mdl->size += size;
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

status_t mdl_copy_in(mdl_t* mdl, size_t count, size_t offset, size_t* copied, const void* source, size_t sourceLength)
{
    if (source == NULL && sourceLength == 0)
    {
        if (copied != NULL)
        {
            *copied = 0;
        }
        return OK;
    }

    if (mdl == NULL || source == NULL)
    {
        if (copied != NULL)
        {
            *copied = 0;
        }
        return ERR(MMU, INVAL);
    }

    count = MIN(count, mdl->size);

    size_t currentOffset = offset;
    size_t start = 0;
    size_t i = 0;
    for (; i < mdl->amount; i++)
    {
        mdl_desc_t* seg = &mdl->descs[i];
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
        mdl_desc_t* seg = &mdl->descs[i];
        size_t toWrite = MIN(remaining, seg->size - segOffset);
        void* addr = PFN_TO_VIRT(seg->pfn) + seg->offset + segOffset;
        memcpy(addr, ptr, toWrite);

        ptr += toWrite;
        remaining -= toWrite;
        segOffset = 0;
        i++;
    }

    if (copied != NULL)
    {
        *copied = count - remaining;
    }

    if (remaining > 0)
    {
        return INFO(MMU, MORE);
    }

    return OK;
}

status_t mdl_copy_out(mdl_t* mdl, size_t count, size_t offset, size_t* copied, void* dest, size_t destLength)
{
    if (mdl == NULL || dest == NULL)
    {
        if (copied != NULL)
        {
            *copied = 0;
        }
        return ERR(MMU, INVAL);
    }

    count = MIN(count, mdl->size);

    size_t currentOffset = offset;
    size_t start = 0;
    size_t i = 0;
    for (; i < mdl->amount; i++)
    {
        mdl_desc_t* seg = &mdl->descs[i];
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
        mdl_desc_t* seg = &mdl->descs[i];
        size_t toRead = MIN(remaining, seg->size - segOffset);
        void* addr = PFN_TO_VIRT(seg->pfn) + seg->offset + segOffset;
        memcpy(ptr, addr, toRead);

        ptr += toRead;
        remaining -= toRead;
        segOffset = 0;
        i++;
    }

    if (copied != NULL)
    {
        *copied = count - remaining;
    }

    return OK;
}

status_t mdl_fill(mdl_t* mdl, size_t count, size_t offset, size_t* filled, uint8_t value)
{
    if (mdl == NULL)
    {
        if (filled != NULL)
        {
            *filled = 0;
        }
        return ERR(MMU, INVAL);
    }

    count = MIN(count, mdl->size);

    size_t currentOffset = offset;
    size_t start = 0;
    size_t i = 0;
    for (; i < mdl->amount; i++)
    {
        mdl_desc_t* seg = &mdl->descs[i];
        if (start + seg->size > currentOffset)
        {
            break;
        }
        start += seg->size;
    }

    size_t remaining = count;
    size_t segOffset = currentOffset - start;

    while (remaining > 0 && i < mdl->amount)
    {
        mdl_desc_t* seg = &mdl->descs[i];
        size_t toFill = MIN(remaining, seg->size - segOffset);
        void* addr = PFN_TO_VIRT(seg->pfn) + seg->offset + segOffset;
        memset(addr, value, toFill);

        remaining -= toFill;
        segOffset = 0;
        i++;
    }

    if (filled != NULL)
    {
        *filled = count - remaining;
    }

    return OK;
}