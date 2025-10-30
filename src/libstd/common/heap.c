#include "heap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/bitmap.h>
#include <sys/io.h>
#include <sys/math.h>
#include <sys/proc.h>
#include <threads.h>

#ifdef _KERNEL_
#include <kernel/log/panic.h>
#include <kernel/mem/vmm.h>
#include <kernel/sync/lock.h>
static lock_t mutex;

void* _heap_map_memory(uint64_t size)
{
    void* addr = vmm_alloc(NULL, NULL, size, PML_PRESENT | PML_WRITE | PML_GLOBAL, VMM_ALLOC_NONE);
    if (addr == NULL)
    {
        return NULL;
    }
    memset(addr, 0, size);

    return addr;
}

void _heap_unmap_memory(void* addr, uint64_t size)
{
    vmm_unmap(NULL, addr, size);
}
#else  // ndef _KERNEL_
static mtx_t mutex;

static fd_t zeroDev = ERR;

void* _heap_map_memory(uint64_t size)
{
    void* addr = mmap(zeroDev, NULL, size, PROT_READ | PROT_WRITE);
    if (addr == NULL)
    {
        return NULL;
    }

    return addr;
}

void _heap_unmap_memory(void* addr, uint64_t size)
{
    munmap(addr, size);
}
#endif // ndef _KERNEL_

static list_t freeLists[_HEAP_NUM_BINS] = {0};
static bitmap_t freeBitmap = {0};
static uint64_t freeBitmapBuffer[BITMAP_BITS_TO_QWORDS(_HEAP_NUM_BINS)] = {0};

list_t _heapList = LIST_CREATE(_heapList);

void _heap_init(void)
{
#ifdef _KERNEL_
    lock_init(&mutex);
#else
    mtx_init(&mutex, mtx_plain);
    zeroDev = open("/dev/zero");
    if (zeroDev == ERR)
    {
        abort();
    }
#endif
    for (uint64_t i = 0; i < _HEAP_NUM_BINS; i++)
    {
        list_init(&freeLists[i]);
    }
    bitmap_init(&freeBitmap, freeBitmapBuffer, _HEAP_NUM_BINS);
}

void _heap_acquire(void)
{
#ifdef _KERNEL_
    lock_acquire(&mutex);
#else
    mtx_lock(&mutex);
#endif
}

void _heap_release(void)
{
#ifdef _KERNEL_
    lock_release(&mutex);
#else
    mtx_unlock(&mutex);
#endif
}

uint64_t _heap_get_bin_index(uint64_t size)
{
    if (size > _HEAP_LARGE_ALLOC_THRESHOLD)
    {
        return ERR;
    }

    if (size < _HEAP_ALIGNMENT)
    {
        return 0;
    }

    return (size / _HEAP_ALIGNMENT) - 1;
}

_heap_header_t* _heap_block_new(uint64_t minSize)
{
    if (minSize == 0)
    {
        return NULL;
    }

    uint64_t totalSize = MAX(sizeof(_heap_header_t) + minSize, PAGE_SIZE);
    uint64_t pageAmount = BYTES_TO_PAGES(totalSize);
    uint64_t alignedTotalSize = pageAmount * PAGE_SIZE;

    _heap_header_t* newBlock = _heap_map_memory(alignedTotalSize);
    if (newBlock == NULL)
    {
        return NULL;
    }
    newBlock->magic = _HEAP_HEADER_MAGIC;
    newBlock->flags = _HEAP_ZEROED;
    newBlock->size = alignedTotalSize - sizeof(_heap_header_t);
    list_entry_init(&newBlock->freeEntry);
    list_entry_init(&newBlock->listEntry);

    _heap_header_t* last = CONTAINER_OF_SAFE(list_last(&_heapList), _heap_header_t, listEntry);
    while (last != NULL && (uintptr_t)last > (uintptr_t)newBlock)
    {
        last = CONTAINER_OF_SAFE(last->listEntry.prev, _heap_header_t, listEntry);
    }

    if (last == NULL)
    {
        list_push(&_heapList, &newBlock->listEntry);
    }
    else
    {
        list_append(&_heapList, &last->listEntry, &newBlock->listEntry);
    }

    return newBlock;
}

void _heap_block_split(_heap_header_t* block, uint64_t newSize)
{
    assert(newSize % _HEAP_ALIGNMENT == 0);
    uint64_t originalTotalSize = sizeof(_heap_header_t) + block->size;
    uint64_t newTotalSize = sizeof(_heap_header_t) + newSize;

    _heap_header_t* remainder = (_heap_header_t*)((uintptr_t)block + newTotalSize);
    remainder->magic = _HEAP_HEADER_MAGIC;
    remainder->flags = block->flags & _HEAP_ZEROED;
    remainder->size = originalTotalSize - newTotalSize - sizeof(_heap_header_t);
    list_entry_init(&remainder->freeEntry);
    list_entry_init(&remainder->listEntry);

    block->size = newSize;

    list_append(&_heapList, &block->listEntry, &remainder->listEntry);

    _heap_free(remainder);
}

void _heap_add_to_free_list(_heap_header_t* block)
{
    uint64_t binIndex = _heap_get_bin_index(block->size);
    if (binIndex == ERR)
    {
        return;
    }
    list_push(&freeLists[binIndex], &block->freeEntry);
    bitmap_set(&freeBitmap, binIndex);
}

void _heap_remove_from_free_list(_heap_header_t* block)
{
    uint64_t binIndex = _heap_get_bin_index(block->size);
    if (binIndex == ERR)
    {
        return;
    }
    list_remove(&freeLists[binIndex], &block->freeEntry);
    if (list_is_empty(&freeLists[binIndex]))
    {
        bitmap_clear(&freeBitmap, binIndex);
    }
}

_heap_header_t* _heap_alloc(uint64_t size)
{
    if (size == 0)
    {
        return NULL;
    }

    if (size > _HEAP_LARGE_ALLOC_THRESHOLD)
    {
        uint64_t totalSize = sizeof(_heap_header_t) + size;
        uint64_t pageAmount = BYTES_TO_PAGES(totalSize);
        uint64_t alignedTotalSize = pageAmount * PAGE_SIZE;

        _heap_header_t* block = _heap_map_memory(alignedTotalSize);
        if (block == NULL)
        {
            return NULL;
        }
        block->magic = _HEAP_HEADER_MAGIC;
        block->flags = _HEAP_ALLOCATED | _HEAP_MAPPED | _HEAP_ZEROED;
        block->size = alignedTotalSize - sizeof(_heap_header_t);
        list_entry_init(&block->freeEntry);
        list_entry_init(&block->listEntry);

        return block;
    }

    size = ROUND_UP(size, _HEAP_ALIGNMENT);

    uint64_t index = _heap_get_bin_index(size);
    _heap_header_t* suitableBlock = NULL;

    uint64_t freeBinIndex = bitmap_find_first_set(&freeBitmap, index);
    if (freeBinIndex != freeBitmap.length)
    {
        suitableBlock = CONTAINER_OF(list_pop(&freeLists[freeBinIndex]), _heap_header_t, freeEntry);
        if (list_is_empty(&freeLists[freeBinIndex]))
        {
            bitmap_clear(&freeBitmap, freeBinIndex);
        }
    }

    if (suitableBlock == NULL)
    {
        suitableBlock = _heap_block_new(size);
        if (suitableBlock == NULL)
        {
            return NULL;
        }
    }

    suitableBlock->flags |= _HEAP_ALLOCATED;
    if (suitableBlock->size >= size + sizeof(_heap_header_t) + _HEAP_ALIGNMENT)
    {
        _heap_block_split(suitableBlock, size);
    }

    return suitableBlock;
}

void _heap_free(_heap_header_t* block)
{
    if (block->flags & _HEAP_MAPPED)
    {
        assert(block->size > _HEAP_LARGE_ALLOC_THRESHOLD);
        _heap_unmap_memory(block, sizeof(_heap_header_t) + block->size);
        return;
    }
    else
    {
        assert(block->size <= _HEAP_LARGE_ALLOC_THRESHOLD);
    }

    block->flags &= ~_HEAP_ALLOCATED;

    _heap_header_t* prev = CONTAINER_OF_SAFE(block->listEntry.prev, _heap_header_t, listEntry);
    _heap_header_t* next = CONTAINER_OF_SAFE(block->listEntry.next, _heap_header_t, listEntry);

    if (next != NULL && !(next->flags & _HEAP_ALLOCATED) && (block->data + block->size == (uint8_t*)next))
    {
        uint64_t newSize = block->size + sizeof(_heap_header_t) + next->size;
        if (newSize <= _HEAP_LARGE_ALLOC_THRESHOLD)
        {
            _heap_remove_from_free_list(next);
            block->size = newSize;
            block->flags = (block->flags & _HEAP_ZEROED) & (next->flags & _HEAP_ZEROED);

            list_remove(&_heapList, &next->listEntry);
        }
    }

    if (prev != NULL && !(prev->flags & _HEAP_ALLOCATED) && (prev->data + prev->size == (uint8_t*)block))
    {
        uint64_t newSize = prev->size + sizeof(_heap_header_t) + block->size;
        if (newSize <= _HEAP_LARGE_ALLOC_THRESHOLD)
        {
            _heap_remove_from_free_list(prev);
            prev->size = newSize;
            prev->flags = (prev->flags & _HEAP_ZEROED) & (block->flags & _HEAP_ZEROED);

            list_remove(&_heapList, &block->listEntry);

            block = prev;
        }
    }

    _heap_add_to_free_list(block);
}
