#include "heap.h"

#include "tty/tty.h"
#include "pmm/pmm.h"
#include "utils/utils.h"
#include "lock/lock.h"
#include "debug/debug.h"
#include "vmm/vmm.h"

static HeapHeader* firstBlock;
static Lock lock;

static inline void heap_split(HeapHeader* block, uint64_t size)
{   
    HeapHeader* newBlock = (HeapHeader*)((uint64_t)block + sizeof(HeapHeader) + size);
    newBlock->size = block->size - sizeof(HeapHeader) - size;
    newBlock->next = block->next;
    newBlock->reserved = 0;
    newBlock->magic = HEAP_HEADER_MAGIC;

    block->size = size;
    block->next = newBlock;
}

static inline HeapHeader* heap_new_block(uint64_t size)
{
    uint64_t pageAmount = PAGE_SIZE_OF(size + sizeof(HeapHeader)) + 1;
    HeapHeader* newBlock = vmm_physical_to_virtual(pmm_allocate_amount(pageAmount));
    newBlock->size = pageAmount * PAGE_SIZE - sizeof(HeapHeader);
    newBlock->next = 0;
    newBlock->reserved = 0;
    newBlock->magic = HEAP_HEADER_MAGIC;

    return newBlock;
}

void heap_init(void)
{    
    firstBlock = heap_new_block(PAGE_SIZE * 64);

    lock = lock_create();
}

uint64_t heap_total_size(void)
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

uint64_t heap_reserved_size(void)
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

uint64_t heap_free_size(void)
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

    size = round_up(size, HEAP_ALIGNMENT);

    HeapHeader* currentBlock = firstBlock;
    while (1) 
    {
        if (!currentBlock->reserved) 
        {
            if (currentBlock->size == size) 
            {
                currentBlock->reserved = 1;

                lock_release(&lock);
                return HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->size > size + sizeof(HeapHeader) + HEAP_ALIGNMENT) 
            {
                heap_split(currentBlock, size);
                currentBlock->reserved = 1;

                lock_release(&lock);
                return HEAP_HEADER_GET_START(currentBlock);
            }
        }

        if (currentBlock->next != 0)
        {
            currentBlock = currentBlock->next;
        }
        else
        {
            break;
        }
    }

    HeapHeader* newBlock = heap_new_block(size);
    currentBlock->next = newBlock;

    if (newBlock->size > size + sizeof(HeapHeader) + HEAP_ALIGNMENT) 
    {
        heap_split(newBlock, size);
    }
    newBlock->reserved = 1;

    lock_release(&lock);
    return HEAP_HEADER_GET_START(newBlock);
}

void kfree(void* ptr)
{
    lock_acquire(&lock);

    HeapHeader* block = (HeapHeader*)((uint64_t)ptr - sizeof(HeapHeader));
    if (block->magic != HEAP_HEADER_MAGIC)
    {
        debug_panic("Invalid heap magic\n");
    }
    else if (!block->reserved)
    {
        debug_panic("Attempt to free unreserved block");
    }
    block->reserved = 0;

    lock_release(&lock);
}