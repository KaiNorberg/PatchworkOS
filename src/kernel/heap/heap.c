#include "heap.h"

#include "tty/tty.h"
#include "page_directory/page_directory.h"
#include "page_allocator/page_allocator.h"
#include "utils/utils.h"
#include "spin_lock/spin_lock.h"

extern uint64_t _kernelEnd;

HeapHeader* firstBlock;

SpinLock heapLock;

void heap_init()
{    
    tty_start_message("Heap initializing");

    heapLock = spin_lock_new();

    void* heapStart = (void*)round_up((uint64_t)&_kernelEnd, 0x1000);

    firstBlock = heapStart;
    page_directory_remap(kernelPageDirectory, heapStart, page_allocator_request(), PAGE_DIR_READ_WRITE);

    firstBlock->size = 0x1000 - sizeof(HeapHeader);
    firstBlock->next = 0;
    firstBlock->reserved = 0;

    tty_end_message(TTY_MESSAGE_OK);
}

void heap_visualize()
{
    Pixel black;
    black.a = 255;
    black.r = 0;
    black.g = 0;
    black.b = 0;

    Pixel green;
    green.a = 255;
    green.r = 152;
    green.g = 195;
    green.b = 121;

    Pixel red;
    red.a = 255;
    red.r = 224;
    red.g = 108;
    red.b = 117;

    tty_print("Heap Visualization:\n\r");

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {   
        if (currentBlock->reserved)
        {
            tty_set_background(red);
        }
        else
        {
            tty_set_background(green);
        }

        tty_put(' '); tty_printi(currentBlock->size); tty_print(" B "); 

        if (currentBlock->next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->next;       
        }
    }
    
    tty_set_background(black);
    tty_print("\n\n\r");
}

uint64_t heap_total_size()
{
    uint64_t size = 0;

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {   
        size += currentBlock->size + sizeof(HeapHeader);

        if (currentBlock->next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->next;       
        }
    }

    return size;
}

uint64_t heap_reserved_size()
{
    uint64_t size = 0;

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {   
        if (currentBlock->reserved)
        {
            size += currentBlock->size + sizeof(HeapHeader);
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

    return size;
}

uint64_t heap_free_size()
{
    uint64_t size = 0;

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {   
        if (!currentBlock->reserved)
        {
            size += currentBlock->size + sizeof(HeapHeader);
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

    return size;
}

uint64_t heap_block_count()
{
    uint64_t count = 0;

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {       
        count++;

        if (currentBlock->next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->next;       
        }
    }

    return count;    
}

HeapHeader* heap_split(HeapHeader* block, uint64_t size)
{   
    uint64_t newSize = block->size - sizeof(HeapHeader) - size;

    HeapHeader* newBlock = (HeapHeader*)((uint64_t)HEAP_HEADER_GET_START(block) + newSize);
    newBlock->next = block->next;
    newBlock->size = size;
    newBlock->reserved = 0;

    block->next = newBlock;
    block->size = newSize;

    return newBlock;
}

void* kmalloc(uint64_t size)
{
    if (size == 0)
    {
        return 0;
    }
    spin_lock_acquire(&heapLock);

    uint64_t alignedSize = round_up(size, 64);

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {
        if (!currentBlock->reserved)
        {
            if (currentBlock->size == alignedSize)
            {
                currentBlock->reserved = 1;

                spin_lock_release(&heapLock);
                return HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->size > alignedSize + sizeof(HeapHeader) + 64)
            {
                HeapHeader* newBlock = heap_split(currentBlock, alignedSize);
                newBlock->reserved = 1;   

                spin_lock_release(&heapLock);
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
    
    HeapHeader* newBlock = (HeapHeader*)HEAP_HEADER_GET_END(currentBlock);

    uint64_t pageAmount = GET_SIZE_IN_PAGES(alignedSize + sizeof(HeapHeader)) + 1;
    for (uint64_t address = (uint64_t)newBlock; address < (uint64_t)newBlock + pageAmount * 0x1000; address += 0x1000)
    {   
        page_directory_remap(kernelPageDirectory, (void*)address, page_allocator_request(), PAGE_DIR_READ_WRITE);
    }

    newBlock->size = pageAmount * 0x1000 - sizeof(HeapHeader);
    newBlock->next = 0;
    newBlock->reserved = 0;

    currentBlock->next = newBlock;

    HeapHeader* splitBlock = heap_split(newBlock, alignedSize);
    splitBlock->reserved = 1;

    spin_lock_release(&heapLock);
    return HEAP_HEADER_GET_START(splitBlock);
}

void kfree(void* ptr)
{    
    spin_lock_acquire(&heapLock);

    HeapHeader* block = (HeapHeader*)((uint64_t)ptr - sizeof(HeapHeader));
    
    uint8_t blockFound = 0;

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {
        if (block == currentBlock)
        {
            blockFound = 1;
            currentBlock->reserved = 0;
            break;
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

    if (!blockFound)
    {
        tty_print("Failed to free block!\n\r");
    }

    while (1)
    {
        uint8_t blocksMerged = 0;

        currentBlock = firstBlock;
        while (1)
        {               
            if (currentBlock->next != 0 && !currentBlock->reserved && !currentBlock->next->reserved)
            {
                currentBlock->size += currentBlock->next->size + sizeof(HeapHeader);
                currentBlock->next = currentBlock->next->next;
                blocksMerged = 1;
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

        if (!blocksMerged)
        {
            break;
        }       
    }    
    
    spin_lock_release(&heapLock);
}