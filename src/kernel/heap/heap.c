#include "heap.h"

#include "tty/tty.h"
#include "page_directory/page_directory.h"
#include "pmm/pmm.h"
#include "utils/utils.h"
#include "lock/lock.h"
#include "debug/debug.h"
#include "vmm/vmm.h"

//TODO: Replace all of this with a better heap, its way to slow.

static HeapHeader* firstBlock;
static Lock lock;

static inline HeapHeader* heap_split(HeapHeader* block, uint64_t size)
{   
    uint64_t newSize = block->size - sizeof(HeapHeader) - size;

    if (size < 64 || newSize < 64)
    {
        debug_panic("Invalid heap split");
    }

    HeapHeader* newBlock = (HeapHeader*)((uint64_t)HEAP_HEADER_GET_START(block) + newSize);
    newBlock->next = block->next;
    newBlock->size = size;
    newBlock->reserved = 0;

    block->next = newBlock;
    block->size = newSize;

    return newBlock;
}

void heap_init()
{    
    firstBlock = vmm_allocate(1);
    firstBlock->size = PAGE_SIZE - sizeof(HeapHeader);
    firstBlock->next = 0;
    firstBlock->reserved = 0;

    lock = lock_new();
}

uint64_t heap_total_size()
{
    uint64_t size = 0;

    HeapHeader* currentBlock = firstBlock;
    while (currentBlock != 0)
    {   
        size += currentBlock->size + sizeof(HeapHeader);

        currentBlock = currentBlock->next;       
    }

    return size;
}

uint64_t heap_reserved_size()
{
    uint64_t size = 0;

    HeapHeader* currentBlock = firstBlock;
    while (currentBlock != 0)
    {   
        if (currentBlock->reserved)
        {
            size += currentBlock->size + sizeof(HeapHeader);
        }

        currentBlock = currentBlock->next;       
    }

    return size;
}

uint64_t heap_free_size()
{
    uint64_t size = 0;

    HeapHeader* currentBlock = firstBlock;
    while (currentBlock != 0)
    {   
        if (!currentBlock->reserved)
        {
            size += currentBlock->size + sizeof(HeapHeader);
        }

        currentBlock = currentBlock->next;       
    }

    return size;
}

void* kmalloc(uint64_t size)
{
    if (size == 0)
    {
        return 0;
    }
    lock_acquire(&lock);

    uint64_t alignedSize = round_up(size, 64);

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {
        if (!currentBlock->reserved)
        {
            if (currentBlock->size == alignedSize)
            {
                currentBlock->reserved = 1;

                lock_release(&lock);
                return HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->size > alignedSize + sizeof(HeapHeader) + 64)
            {
                HeapHeader* newBlock = heap_split(currentBlock, alignedSize);
                newBlock->reserved = 1;   

                lock_release(&lock);
                return HEAP_HEADER_GET_START(newBlock);
            }
        }

        if (currentBlock->next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->next;       
        }
    }

    uint64_t pageAmount = SIZE_IN_PAGES(alignedSize + sizeof(HeapHeader)) + 1;
    HeapHeader* newBlock = vmm_allocate(pageAmount);

    newBlock->size = pageAmount * PAGE_SIZE - sizeof(HeapHeader);
    newBlock->next = 0;
    newBlock->reserved = 0;

    currentBlock->next = newBlock;

    if (newBlock->size > alignedSize + sizeof(HeapHeader) + 64)
    {
        HeapHeader* splitBlock = heap_split(newBlock, alignedSize);
        splitBlock->reserved = 1;

        lock_release(&lock);
        return HEAP_HEADER_GET_START(splitBlock);
    }
    else
    {
        newBlock->reserved = 1;
        
        lock_release(&lock);
        return HEAP_HEADER_GET_START(newBlock);
    }
}

void kfree(void* ptr)
{    
    //TODO: Optimize this!

    lock_acquire(&lock);

    HeapHeader const* block = (HeapHeader*)((uint64_t)ptr - sizeof(HeapHeader));
    
    HeapHeader* currentBlock = firstBlock;
    while (currentBlock != 0)
    {
        if (block == currentBlock && currentBlock->reserved)
        {
            currentBlock->reserved = 0;
            lock_release(&lock);
            return;
        }

        currentBlock = currentBlock->next;
    }

    debug_panic("Failed to free block!\n");

    lock_release(&lock);
}