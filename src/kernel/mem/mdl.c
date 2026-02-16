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
        pmm_ref_dec(mdl->entries[i].pfn, BYTES_TO_PAGES(mdl->entries[i].offset + mdl->entries[i].size));
    }
    mdl->amount = 0;

    if (mdl->entries != mdl->small)
    {
        free(mdl->entries);
    }
    mdl->entries = NULL;
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
        uint32_t newCapacity = mdl->capacity * 2;
        mdl_entry_t* newEntries;

        if (mdl->entries == mdl->small)
        {
            newEntries = malloc(newCapacity * sizeof(mdl_entry_t));
            if (newEntries != NULL)
            {
                memcpy(newEntries, mdl->small, sizeof(mdl->small));
            }
        }
        else
        {
            newEntries = realloc(mdl->entries, newCapacity * sizeof(mdl_entry_t));
        }

        if (newEntries == NULL)
        {
            return ERR(MMU, NOMEM);
        }

        mdl->entries = newEntries;
        mdl->capacity = newCapacity;
    }

    mdl_entry_t* entry = &mdl->entries[mdl->amount];
    pfn_t pfn = PHYS_TO_PFN(phys);
    uint32_t offset = phys % PAGE_SIZE;
    if (pmm_ref_inc(pfn, BYTES_TO_PAGES(offset + size)) == 0)
    {
        return ERR(MMU, FAULT);
    }

    entry->pfn = pfn;
    entry->size = size;
    entry->offset = offset;

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

status_t mdl_add_vector(mdl_t* mdl, space_t* space, const iovec_t* vector, size_t count)
{
    if (mdl == NULL)
    {
        return ERR(MMU, INVAL);
    }

    if (count == 0)
    {
        return OK;
    }

    if (vector == NULL)
    {
        return ERR(MMU, INVAL);
    }

    for (size_t i = 0; i < count; i++)
    {
        iovec_t vec;
        const uint8_t* src = (const uint8_t*)&vector[i];
        uint8_t* dst = (uint8_t*)&vec;
        size_t len = sizeof(iovec_t);

        while (len > 0)
        {
            phys_addr_t phys;
            status_t status = space_virt_to_phys(&phys, space, src);
            if (IS_ERR(status))
            {
                return status;
            }

            size_t offset = phys % PAGE_SIZE;
            size_t avail = MIN(len, PAGE_SIZE - offset);

            void* kaddr = PFN_TO_VIRT(PHYS_TO_PFN(phys)) + offset;
            memcpy(dst, kaddr, avail);

            dst += avail;
            src += avail;
            len -= avail;
        }

        status_t status = mdl_add(mdl, space, vec.base, vec.length);
        if (IS_ERR(status))
        {
            return status;
        }
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
        mdl_entry_t* entry = &mdl->entries[i];
        if (start + entry->size > currentOffset)
        {
            break;
        }
        start += entry->size;
    }

    const uint8_t* ptr = source;
    size_t copySize = MIN(sourceLength, count);
    size_t remaining = copySize;

    size_t entryOffset = currentOffset - start;
    while (remaining > 0 && i < mdl->amount)
    {
        mdl_entry_t* entry = &mdl->entries[i];
        size_t toWrite = MIN(remaining, entry->size - entryOffset);
        void* addr = PFN_TO_VIRT(entry->pfn) + entry->offset + entryOffset;
        memcpy(addr, ptr, toWrite);

        ptr += toWrite;
        remaining -= toWrite;
        entryOffset = 0;
        i++;
    }

    if (copied != NULL)
    {
        *copied = copySize - remaining;
    }

    if (remaining > 0)
    {
        return INFO(MMU, MORE);
    }

    return OK;
}

status_t mdl_copy_out(mdl_t* mdl, size_t count, size_t offset, size_t* copied, void* dest, size_t destLength)
{
    if (dest == NULL && destLength == 0)
    {
        if (copied != NULL)
        {
            *copied = 0;
        }
        return OK;
    }

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
        mdl_entry_t* entry = &mdl->entries[i];
        if (start + entry->size > currentOffset)
        {
            break;
        }
        start += entry->size;
    }

    uint8_t* ptr = dest;
    size_t copySize = MIN(destLength, count);
    size_t remaining = copySize;

    size_t entryOffset = currentOffset - start;
    while (remaining > 0 && i < mdl->amount)
    {
        mdl_entry_t* entry = &mdl->entries[i];
        size_t toRead = MIN(remaining, entry->size - entryOffset);
        void* addr = PFN_TO_VIRT(entry->pfn) + entry->offset + entryOffset;
        memcpy(ptr, addr, toRead);

        ptr += toRead;
        remaining -= toRead;
        entryOffset = 0;
        i++;
    }

    if (copied != NULL)
    {
        *copied = copySize - remaining;
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
        mdl_entry_t* entry = &mdl->entries[i];
        if (start + entry->size > currentOffset)
        {
            break;
        }
        start += entry->size;
    }

    size_t remaining = count;
    size_t entryOffset = currentOffset - start;

    while (remaining > 0 && i < mdl->amount)
    {
        mdl_entry_t* entry = &mdl->entries[i];
        size_t toFill = MIN(remaining, entry->size - entryOffset);
        void* addr = PFN_TO_VIRT(entry->pfn) + entry->offset + entryOffset;
        memset(addr, value, toFill);

        remaining -= toFill;
        entryOffset = 0;
        i++;
    }

    if (filled != NULL)
    {
        *filled = count - remaining;
    }

    return OK;
}