#include <kernel/mem/paging_types.h>
#include <kernel/mem/sglist.h>
#include <kernel/mem/space.h>
#include <kernel/proc/process.h>

#include <stdlib.h>
#include <string.h>
#include <sys/proc.h>

void sglist_deinit(sglist_t* list)
{
    if (list == NULL)
    {
        return;
    }

    list->next = NULL;

    for (size_t i = 0; i < list->amount; i++)
    {
        pmm_ref_dec(list->entries[i].pfn, BYTES_TO_PAGES(list->entries[i].offset + list->entries[i].size));
    }
    list->amount = 0;

    if (list->entries != list->small)
    {
        free(list->entries);
    }
    list->entries = NULL;
    list->capacity = 0;
    list->size = 0;
}

void sglist_free_chain(sglist_t* list, void (*free)(void*))
{
    while (list != NULL)
    {
        sglist_t* next = list->next;
        sglist_deinit(list);
        if (free != NULL)
        {
            free(list);
        }
        list = next;
    }
}

static status_t sglist_push(sglist_t* list, phys_addr_t phys, size_t size)
{
    if (size > UINT32_MAX)
    {
        return ERR(MMU, TOOBIG);
    }

    if (list == NULL)
    {
        return ERR(MMU, INVAL);
    }

    if (list->amount == list->capacity)
    {
        uint32_t newCapacity = list->capacity * 2;
        sglist_entry_t* newEntries;

        if (list->entries == list->small)
        {
            newEntries = malloc(newCapacity * sizeof(sglist_entry_t));
            if (newEntries != NULL)
            {
                memcpy(newEntries, list->small, sizeof(list->small));
            }
        }
        else
        {
            newEntries = realloc(list->entries, newCapacity * sizeof(sglist_entry_t));
        }

        if (newEntries == NULL)
        {
            return ERR(MMU, NOMEM);
        }

        list->entries = newEntries;
        list->capacity = newCapacity;
    }

    sglist_entry_t* entry = &list->entries[list->amount];
    pfn_t pfn = PHYS_TO_PFN(phys);
    uint32_t offset = phys % PAGE_SIZE;
    if (pmm_ref_inc(pfn, BYTES_TO_PAGES(offset + size)) == 0)
    {
        return ERR(MMU, FAULT);
    }

    entry->pfn = pfn;
    entry->size = size;
    entry->offset = offset;

    list->amount++;
    list->size += size;
    return OK;
}

status_t sglist_add(sglist_t* list, space_t* space, const void* addr, size_t size)
{
    const uint8_t* ptr = addr;
    size_t remaining = size;

    while (remaining > 0)
    {
        phys_addr_t phys;
        status_t status = space_virt_to_phys(space, ptr, &phys);
        if (IS_ERR(status))
        {
            return status;
        }

        size_t offset = phys % PAGE_SIZE;
        size_t len = MIN(remaining, PAGE_SIZE - offset);

        status = sglist_push(list, phys, len);
        if (IS_ERR(status))
        {
            return status;
        }

        ptr += len;
        remaining -= len;
    }

    return OK;
}

status_t sglist_add_vector(sglist_t* list, space_t* space, const iovec_t* vector, size_t count)
{
    if (list == NULL)
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
        status_t status = space_copy_out(space, &vec, &vector[i], sizeof(iovec_t));
        if (IS_ERR(status))
        {
            return status;
        }

        status = sglist_add(list, space, vec.base, vec.length);
        if (IS_ERR(status))
        {
            return status;
        }
    }

    return OK;
}

status_t sglist_copy_in(sglist_t* list, size_t count, size_t offset, size_t* copied, const void* source,
    size_t sourceLength)
{
    if (source == NULL && sourceLength == 0)
    {
        if (copied != NULL)
        {
            *copied = 0;
        }
        return OK;
    }

    if (list == NULL || source == NULL)
    {
        if (copied != NULL)
        {
            *copied = 0;
        }
        return ERR(MMU, INVAL);
    }

    count = MIN(count, list->size);

    size_t currentOffset = offset;
    size_t start = 0;
    size_t i = 0;
    for (; i < list->amount; i++)
    {
        sglist_entry_t* entry = &list->entries[i];
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
    while (remaining > 0 && i < list->amount)
    {
        sglist_entry_t* entry = &list->entries[i];
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

    return OK;
}

status_t sglist_copy_out(sglist_t* list, size_t count, size_t offset, size_t* copied, void* dest, size_t destLength)
{
    if (dest == NULL && destLength == 0)
    {
        if (copied != NULL)
        {
            *copied = 0;
        }
        return OK;
    }

    if (list == NULL || dest == NULL)
    {
        if (copied != NULL)
        {
            *copied = 0;
        }
        return ERR(MMU, INVAL);
    }

    count = MIN(count, list->size);

    size_t currentOffset = offset;
    size_t start = 0;
    size_t i = 0;
    for (; i < list->amount; i++)
    {
        sglist_entry_t* entry = &list->entries[i];
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
    while (remaining > 0 && i < list->amount)
    {
        sglist_entry_t* entry = &list->entries[i];
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

status_t sglist_fill(sglist_t* list, size_t count, size_t offset, size_t* filled, uint8_t value)
{
    if (list == NULL)
    {
        if (filled != NULL)
        {
            *filled = 0;
        }
        return ERR(MMU, INVAL);
    }

    count = MIN(count, list->size);

    size_t currentOffset = offset;
    size_t start = 0;
    size_t i = 0;
    for (; i < list->amount; i++)
    {
        sglist_entry_t* entry = &list->entries[i];
        if (start + entry->size > currentOffset)
        {
            break;
        }
        start += entry->size;
    }

    size_t remaining = count;
    size_t entryOffset = currentOffset - start;

    while (remaining > 0 && i < list->amount)
    {
        sglist_entry_t* entry = &list->entries[i];
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

status_t sglist_copy_in_pagevec(sglist_t* list, size_t count, size_t listOffset, size_t* copied, const pagevec_t* vec,
    size_t vecOffset)
{
    size_t remaining = count;
    size_t currentListOffset = listOffset;
    size_t currentVecOffset = vecOffset;
    size_t totalCopied = 0;

    while (remaining > 0)
    {
        size_t pageIdx = currentVecOffset / PAGE_SIZE;
        size_t pageOff = currentVecOffset % PAGE_SIZE;
        size_t chunk = MIN(remaining, PAGE_SIZE - pageOff);

        if (pageIdx >= vec->amount)
        {
            break;
        }

        size_t bytesCopied = 0;
        status_t status = sglist_copy_in(list, chunk, currentListOffset, &bytesCopied,
            (uint8_t*)PFN_TO_VIRT(vec->pfns[pageIdx]) + pageOff, chunk);
        if (IS_ERR(status))
        {
            if (copied != NULL)
            {
                *copied = totalCopied;
            }
            return status;
        }

        totalCopied += bytesCopied;
        currentListOffset += bytesCopied;
        currentVecOffset += bytesCopied;
        remaining -= bytesCopied;

        if (IS_CODE(status, EOF) || bytesCopied < chunk)
        {
            break;
        }
    }

    if (copied != NULL)
    {
        *copied = totalCopied;
    }

    return OK;
}

status_t sglist_copy_out_pagevec(sglist_t* list, size_t count, size_t listOffset, size_t* copied, pagevec_t* vec,
    size_t vecOffset)
{
    size_t remaining = count;
    size_t currentListOffset = listOffset;
    size_t currentVecOffset = vecOffset;
    size_t totalCopied = 0;
    while (remaining > 0)
    {
        size_t pageIdx = currentVecOffset / PAGE_SIZE;
        size_t pageOff = currentVecOffset % PAGE_SIZE;
        size_t chunk = MIN(remaining, PAGE_SIZE - pageOff);

        if (pageIdx >= vec->amount)
        {
            break;
        }

        size_t bytesCopied = 0;
        status_t status = sglist_copy_out(list, chunk, currentListOffset, &bytesCopied,
            (uint8_t*)PFN_TO_VIRT(vec->pfns[pageIdx]) + pageOff, chunk);
        if (IS_ERR(status))
        {
            if (copied != NULL)
            {
                *copied = totalCopied;
            }
            return status;
        }

        totalCopied += bytesCopied;
        currentListOffset += bytesCopied;
        currentVecOffset += bytesCopied;
        remaining -= bytesCopied;

        if (bytesCopied < chunk)
        {
            break;
        }
    }

    if (copied != NULL)
    {
        *copied = totalCopied;
    }

    return OK;
}

status_t sglist_copy_in_space(sglist_t* list, size_t count, size_t listOffset, size_t* copied, space_t* space,
    const void* source)
{
    size_t remaining = count;
    size_t currentListOffset = listOffset;
    size_t totalCopied = 0;
    const uint8_t* ptr = source;

    while (remaining > 0)
    {
        phys_addr_t phys;
        status_t status = space_virt_to_phys(space, ptr, &phys);
        if (IS_ERR(status))
        {
            if (copied != NULL)
            {
                *copied = totalCopied;
            }
            return status;
        }

        size_t pageOffset = phys % PAGE_SIZE;
        size_t chunk = MIN(remaining, PAGE_SIZE - pageOffset);
        void* kaddr = PHYS_TO_VIRT(phys) + pageOffset;

        size_t bytesCopied = 0;
        status = sglist_copy_in(list, chunk, currentListOffset, &bytesCopied, kaddr, chunk);
        if (IS_ERR(status))
        {
            if (copied != NULL)
            {
                *copied = totalCopied;
            }
            return status;
        }

        totalCopied += bytesCopied;
        currentListOffset += bytesCopied;
        ptr += bytesCopied;
        remaining -= bytesCopied;

        if (IS_CODE(status, EOF) || bytesCopied < chunk)
        {
            break;
        }
    }

    if (copied != NULL)
    {
        *copied = totalCopied;
    }
    return OK;
}

status_t sglist_copy_out_space(sglist_t* list, size_t count, size_t listOffset, size_t* copied, space_t* space,
    void* dest)
{
    size_t remaining = count;
    size_t currentListOffset = listOffset;
    size_t totalCopied = 0;
    uint8_t* ptr = dest;

    while (remaining > 0)
    {
        phys_addr_t phys;
        status_t status = space_virt_to_phys(space, ptr, &phys);
        if (IS_ERR(status))
        {
            if (copied != NULL)
            {
                *copied = totalCopied;
            }
            return status;
        }

        size_t pageOffset = phys % PAGE_SIZE;
        size_t chunk = MIN(remaining, PAGE_SIZE - pageOffset);
        void* kaddr = PHYS_TO_VIRT(phys) + pageOffset;

        size_t bytesCopied = 0;
        status = sglist_copy_out(list, chunk, currentListOffset, &bytesCopied, kaddr, chunk);
        if (IS_ERR(status))
        {
            if (copied != NULL)
            {
                *copied = totalCopied;
            }
            return status;
        }

        totalCopied += bytesCopied;
        currentListOffset += bytesCopied;
        ptr += bytesCopied;
        remaining -= bytesCopied;

        if (bytesCopied < chunk)
        {
            break;
        }
    }

    if (copied != NULL)
    {
        *copied = totalCopied;
    }
    return OK;
}