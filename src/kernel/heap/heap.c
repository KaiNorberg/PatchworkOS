#include "heap.h"

#include "tty/tty.h"
#include "debug/debug.h"

#define HEAP_HEADER_GET_START(block) ((void*)((uint64_t)block + sizeof(BlockHeader)))
#define HEAP_HEADER_GET_END(block) ((void*)((uint64_t)block + sizeof(BlockHeader) + block->Size))

typedef struct BlockHeader
{
    struct BlockHeader* Next;
    uint64_t Size;
    uint8_t Reserved;
    uint8_t AtPageStart;
} BlockHeader;

BlockHeader* firstBlock;
BlockHeader* lastBlock;

void heap_init()
{    
    tty_start_message("Heap initializing");

    firstBlock = (BlockHeader*)page_allocator_request(); 
    lastBlock = firstBlock;

    firstBlock->Size = 0x1000 - sizeof(BlockHeader);
    firstBlock->Next = 0;
    firstBlock->Reserved = 0;
    firstBlock->AtPageStart = 1;

    tty_end_message(TTY_MESSAGE_OK);
}

void heap_visualize()
{
    Pixel black;
    black.A = 255;
    black.R = 0;
    black.G = 0;
    black.B = 0;

    Pixel green;
    green.A = 255;
    green.R = 152;
    green.G = 195;
    green.B = 121;

    Pixel red;
    red.A = 255;
    red.R = 224;
    red.G = 108;
    red.B = 117;

    tty_print("Heap Visualization:\n\r");

    BlockHeader* currentBlock = firstBlock;
    while (1)
    {   
        if (currentBlock->AtPageStart && currentBlock != firstBlock)
        {
            tty_set_background(black);
            tty_put(' ');
        }

        if (currentBlock->Reserved)
        {
            tty_set_background(red);
        }
        else
        {
            tty_set_background(green);
        }

        tty_put(' '); tty_printi(currentBlock->Size); tty_print(" B "); 

        if (currentBlock->Next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->Next;       
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
        size += currentBlock->Size + sizeof(BlockHeader);

        if (currentBlock->Next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->Next;       
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
        if (currentBlock->Reserved)
        {
            size += currentBlock->Size + sizeof(BlockHeader);
        }

        if (currentBlock->Next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->Next;       
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
        if (!currentBlock->Reserved)
        {
            size += currentBlock->Size + sizeof(BlockHeader);
        }

        if (currentBlock->Next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->Next;       
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

        if (currentBlock->Next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->Next;       
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
        if (!currentBlock->Reserved)
        {
            if (currentBlock->Size == alignedSize)
            {
                currentBlock->Reserved = 1;
                return HEAP_HEADER_GET_START(currentBlock);
            }
            else if (currentBlock->Size > alignedSize)
            {
                uint64_t newSize = currentBlock->Size - alignedSize - sizeof(BlockHeader);

                if (currentBlock->Size - newSize >= 64 && newSize >= 64)
                {
                    BlockHeader* newBlock = (BlockHeader*)((uint64_t)HEAP_HEADER_GET_START(currentBlock) + alignedSize);
                    newBlock->Next = currentBlock->Next;
                    newBlock->Size = newSize;
                    newBlock->Reserved = 0;
                    newBlock->AtPageStart = 0;

                    currentBlock->Next = newBlock;
                    currentBlock->Size = alignedSize;
                    currentBlock->Reserved = 1;

                    if (currentBlock == lastBlock)
                    {
                        lastBlock = newBlock;
                    }

                    return HEAP_HEADER_GET_START(currentBlock);
                }
            }
        }

        if (currentBlock->Next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->Next;       
        }
    }

    uint64_t pageAmount = (size + sizeof(BlockHeader)) / 0x1000 + 1;
    BlockHeader* newBlock = (BlockHeader*)page_allocator_request_amount(pageAmount);

    newBlock->Size = pageAmount * 0x1000 - sizeof(BlockHeader);
    newBlock->Next = 0;
    newBlock->Reserved = 0;
    newBlock->AtPageStart = 1;
    
    lastBlock->Next = newBlock;
    lastBlock = newBlock;

    return kmalloc(size);
}

void kfree(void* ptr)
{
    BlockHeader* block = (BlockHeader*)((uint64_t)ptr - sizeof(BlockHeader));
    
    uint8_t blockFound = 0;

    BlockHeader* currentBlock = firstBlock;
    while (1)
    {
        if (block == currentBlock)
        {
            blockFound = 1;
            currentBlock->Reserved = 0;
        }

        if (currentBlock->Next == 0)
        {
            break;
        }
        else
        {
            currentBlock = currentBlock->Next;       
        }    
    }

    if (!blockFound)
    {    
        debug_error("Failed to free block!\n\r");
    }

    while (1)
    {
        uint8_t mergedBlocks = 0;

        currentBlock = firstBlock;
        while (1)
        {   
            if (!currentBlock->Reserved)
            {
                uint64_t contiguousSize = currentBlock->Size;
                BlockHeader* firstFreeContiguousBlock = currentBlock;
                BlockHeader* lastFreeContiguousBlock = currentBlock;
                while (1)
                {
                    BlockHeader* nextBlock = lastFreeContiguousBlock->Next;
                    if (nextBlock == 0 || nextBlock->Reserved || nextBlock->AtPageStart)
                    {
                        break;
                    }
                    else
                    {
                        contiguousSize += nextBlock->Size + sizeof(BlockHeader);
                        lastFreeContiguousBlock = nextBlock;
                    }
                }

                if (firstFreeContiguousBlock != lastFreeContiguousBlock)
                {
                    firstFreeContiguousBlock->Next = lastFreeContiguousBlock->Next;
                    firstFreeContiguousBlock->Size = contiguousSize;
                    mergedBlocks = 1;
                }

                currentBlock = lastFreeContiguousBlock;
            }            

            if (currentBlock->Next == 0)
            {
                break;
            }
            else
            {
                currentBlock = currentBlock->Next;       
            }    
        }

        if (!mergedBlocks)
        {
            break;
        }       
    }
}