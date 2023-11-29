#include "heap.h"

#include "kernel/tty/tty.h"

#define HEAP_HEADER_GET_START(block) ((void*)((uint64_t)block + sizeof(BlockHeader)))
#define HEAP_HEADER_GET_END(block) ((void*)((uint64_t)block + sizeof(BlockHeader) + block->Size))

typedef struct BlockHeader
{
    struct BlockHeader* Next;
    uint64_t Size;
    uint8_t Reserved;
} BlockHeader;

BlockHeader* firstBlock;
BlockHeader* lastBlock;

void heap_init(uint64_t heapStart, uint64_t startSize)
{    
    tty_start_message("Heap initializing");

    if (startSize % 4096 != 0)
    {
        tty_end_message(TTY_MESSAGE_ER);
        return;
    }

    firstBlock = (BlockHeader*)heapStart;    
    lastBlock = firstBlock;

    uint64_t startPageAmount = (startSize + sizeof(BlockHeader)) / 4096 + 1;
    for (uint64_t address = (uint64_t)firstBlock; address < (uint64_t)firstBlock + startPageAmount * 4096; address += 4096)
    {
        virtual_memory_remap_current((void*)address, page_allocator_request());
    }

    firstBlock->Size = startSize;
    firstBlock->Next = 0;
    firstBlock->Reserved = 0;

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

    BlockHeader* currentBlock = firstBlock;
    while (1)
    {   
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
    tty_print("\n\r");
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

void heap_reserve(uint64_t size)
{    
    size += sizeof(BlockHeader) + 4096;
    size = size + (4096 - (size % 4096));

    BlockHeader* newBlock = (BlockHeader*)HEAP_HEADER_GET_END(lastBlock);

    for (uint64_t address = (uint64_t)newBlock; address < (uint64_t)newBlock + size; address += 4096)
    {
        virtual_memory_remap_current((void*)address, page_allocator_request());
    }

    newBlock->Size = size;
    newBlock->Next = 0;
    newBlock->Reserved = 0;

    lastBlock->Next = newBlock;
    
    lastBlock = newBlock;

    heap_visualize();
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

                if (currentBlock->Size >= 64 && newSize >= 4096)
                {
                    BlockHeader* newBlock = (BlockHeader*)((uint64_t)HEAP_HEADER_GET_START(currentBlock) + alignedSize);
                    newBlock->Next = currentBlock->Next;
                    newBlock->Size = newSize;
                    newBlock->Reserved = 0;

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
    
    if (lastBlock->Reserved) //Create new block
    {
        BlockHeader* newBlock = (BlockHeader*)HEAP_HEADER_GET_END(lastBlock);

        for (uint64_t address = (uint64_t)newBlock; address < (uint64_t)newBlock + alignedSize + sizeof(BlockHeader); address += 4096)
        {
            virtual_memory_remap_current((void*)address, page_allocator_request());
        }

        newBlock->Size = alignedSize;
        newBlock->Next = 0;
        newBlock->Reserved = 1;

        lastBlock->Next = newBlock;
        
        lastBlock = newBlock;

        return HEAP_HEADER_GET_START(newBlock);
    }
    else //Expand last block
    {        
        uint64_t sizeDelta = alignedSize - lastBlock->Size;

        void* lastBlockEnd = HEAP_HEADER_GET_END(lastBlock);
        for (uint64_t address = (uint64_t)lastBlockEnd; address < (uint64_t)lastBlockEnd + sizeDelta; address += 4096)
        {
            virtual_memory_remap_current((void*)address, page_allocator_request());
        }

        lastBlock->Size = alignedSize; 
        lastBlock->Reserved = 1;

        return HEAP_HEADER_GET_START(lastBlock);
    }
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
        tty_print("Failed to free block!\n\r");
    }

    while (1)
    {
        uint8_t blocksMerged = 0;

        currentBlock = firstBlock;
        while (1)
        {               
            if (currentBlock->Next != 0 && !currentBlock->Reserved && !currentBlock->Next->Reserved)
            {
                if (currentBlock->Next == lastBlock)
                {
                    lastBlock = currentBlock;
                }

                currentBlock->Size += currentBlock->Next->Size + sizeof(BlockHeader);
                currentBlock->Next = currentBlock->Next->Next;
                blocksMerged = 1;
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

        if (!blocksMerged)
        {
            break;
        }       
    }
}