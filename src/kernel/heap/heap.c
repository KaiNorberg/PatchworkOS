#include "heap.h"

#include "tty/tty.h"
#include "debug/debug.h"

#define HEAP_HEADER_GET_START(block) ((void*)((uint64_t)block + sizeof(BlockHeader)))
#define HEAP_HEADER_GET_END(block) ((void*)((uint64_t)block + sizeof(BlockHeader) + block->size))

typedef struct BlockHeader
{
    struct BlockHeader* next;
    uint64_t size;
    uint8_t reserved;
    uint8_t atPageStart;
} BlockHeader;

BlockHeader* firstBlock;
BlockHeader* lastBlock;

void heap_init()
{    
    tty_start_message("Heap initializing");

    firstBlock = (BlockHeader*)page_allocator_request(); 
    lastBlock = firstBlock;

    firstBlock->size = 0x1000 - sizeof(BlockHeader);
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

    BlockHeader* currentBlock = firstBlock;
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

    BlockHeader* currentBlock = firstBlock;
    while (1)
    {   
        size += currentBlock->size + sizeof(BlockHeader);

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

    BlockHeader* currentBlock = firstBlock;
    while (1)
    {   
        if (currentBlock->reserved)
        {
            size += currentBlock->size + sizeof(BlockHeader);
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

    BlockHeader* currentBlock = firstBlock;
    while (1)
    {   
        if (!currentBlock->reserved)
        {
            size += currentBlock->size + sizeof(BlockHeader);
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

    BlockHeader* currentBlock = firstBlock;
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

    BlockHeader* currentBlock = firstBlock;
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
                uint64_t newSize = currentBlock->size - alignedSize - sizeof(BlockHeader);

                if (currentBlock->size - newSize >= 64 && newSize >= 64)
                {
                    BlockHeader* newBlock = (BlockHeader*)((uint64_t)HEAP_HEADER_GET_START(currentBlock) + alignedSize);
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

    uint64_t pageAmount = GET_SIZE_IN_PAGES(size + sizeof(BlockHeader));
    BlockHeader* newBlock = (BlockHeader*)page_allocator_request_amount(pageAmount);

    newBlock->size = pageAmount * 0x1000 - sizeof(BlockHeader);
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
    
    BlockHeader* block = (BlockHeader*)((uint64_t)ptr - sizeof(BlockHeader));
    
    uint8_t blockFound = 0;

    //Find and free block
    BlockHeader* currentBlock = firstBlock;
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
                BlockHeader* firstFreeContiguousBlock = currentBlock;
                BlockHeader* lastFreeContiguousBlock = currentBlock;
                while (1)
                {
                    BlockHeader* nextBlock = lastFreeContiguousBlock->next;
                    if (nextBlock == 0 || nextBlock->reserved || nextBlock->atPageStart)
                    {
                        break;
                    }
                    else
                    {
                        contiguousSize += nextBlock->size + sizeof(BlockHeader);
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