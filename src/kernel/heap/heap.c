#include "heap.h"

#include "tty/tty.h"
#include "debug/debug.h"
#include "utils/utils.h"
#include "page_allocator/page_allocator.h"
#include "page_directory/page_directory.h"

HeapHeader* firstBlock;
HeapHeader* lastBlock;

extern uint64_t _kernelEnd;

void heap_init()
{    
    tty_start_message("Heap initializing");

    void* heapStart = (void*)round_up((uint64_t)&_kernelEnd, 0x1000);

    firstBlock = heapStart;
    lastBlock = firstBlock;
    page_directory_remap(kernelPageDirectory, heapStart, page_allocator_request(), PAGE_DIR_READ_WRITE);

    firstBlock->size = 0x1000 - sizeof(HeapHeader);
    firstBlock->next = 0;
    firstBlock->reserved = 0;
    firstBlock->atPageStart = 1;

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
        if (currentBlock->atPageStart && currentBlock != firstBlock)
        {
            tty_set_background(black);
            tty_put(' ');
        }

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

void* kmalloc(uint64_t size)
{
    if (size == 0)
    {
        return 0;
    }

    uint64_t alignedSize = size + (64 - (size % 64));

    HeapHeader* currentBlock = firstBlock;
    while (1)
    {
        if (!currentBlock->reserved)
        {
            if (currentBlock->size == alignedSize)
            {
                currentBlock->reserved = 1;
                return HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->size > alignedSize)
            {
                uint64_t newSize = currentBlock->size - alignedSize - sizeof(HeapHeader);

                if (currentBlock->size - newSize >= 64 && newSize >= 64)
                {
                    HeapHeader* newBlock = (HeapHeader*)((uint64_t)HEAP_HEADER_GET_START(currentBlock) + alignedSize);
                    newBlock->next = currentBlock->next;
                    newBlock->size = newSize;
                    newBlock->reserved = 0;
                    newBlock->atPageStart = 0;

                    currentBlock->next = newBlock;
                    currentBlock->size = alignedSize;
                    currentBlock->reserved = 1;

                    if (currentBlock == lastBlock)
                    {
                        lastBlock = newBlock;
                    }

                    return HEAP_HEADER_GET_START(currentBlock);
                }
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

    uint64_t pageAmount = GET_SIZE_IN_PAGES(size + sizeof(HeapHeader));
    void* physicalAddress = page_allocator_request_amount(pageAmount);
    void* virtualAddress = (void*)round_up((uint64_t)HEAP_HEADER_GET_END(lastBlock), 0x1000);

    page_directory_remap_pages(kernelPageDirectory, virtualAddress, physicalAddress, pageAmount, PAGE_DIR_READ_WRITE);

    HeapHeader* newBlock = virtualAddress;
    newBlock->size = pageAmount * 0x1000 - sizeof(HeapHeader);
    newBlock->next = 0;
    newBlock->reserved = 0;
    newBlock->atPageStart = 1;
    
    lastBlock->next = newBlock;
    lastBlock = newBlock;

    return kmalloc(size);
}

void kfree(void* ptr)
{
    if (ptr == 0)
    {
        debug_panic("Attempted to free null ptr!");
    }
    
    HeapHeader* block = (HeapHeader*)((uint64_t)ptr - sizeof(HeapHeader));
    
    uint8_t blockFound = 0;

    //Find and free block
    HeapHeader* currentBlock = firstBlock;
    while (1)
    {
        if (block == currentBlock)
        {
            blockFound = 1;
            currentBlock->reserved = 0;
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
        debug_panic("Failed to free block!");
    }

    //Merge contiguous free blocks
    while (1)
    {
        uint8_t mergedBlocks = 0;

        currentBlock = firstBlock;
        while (1)
        {   
            if (!currentBlock->reserved)
            {
                uint64_t contiguousSize = currentBlock->size;
                HeapHeader* firstFreeContiguousBlock = currentBlock;
                HeapHeader* lastFreeContiguousBlock = currentBlock;
                while (1)
                {
                    HeapHeader* nextBlock = lastFreeContiguousBlock->next;
                    if (nextBlock == 0 || nextBlock->reserved || nextBlock->atPageStart)
                    {
                        break;
                    }
                    else
                    {
                        contiguousSize += nextBlock->size + sizeof(HeapHeader);
                        lastFreeContiguousBlock = nextBlock;
                    }
                }

                if (firstFreeContiguousBlock != lastFreeContiguousBlock)
                {
                    firstFreeContiguousBlock->next = lastFreeContiguousBlock->next;
                    firstFreeContiguousBlock->size = contiguousSize;
                    mergedBlocks = 1;
                }

                currentBlock = lastFreeContiguousBlock;
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

        if (!mergedBlocks)
        {
            break;
        }       
    }
}