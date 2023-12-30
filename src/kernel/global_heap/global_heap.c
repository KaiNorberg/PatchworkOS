#include "global_heap.h"

#include "page_allocator/page_allocator.h"
#include "string/string.h"
#include "tty/tty.h"
#include "debug/debug.h"

#include "../common.h"

extern uint64_t _kernelStart;

GlobalHeapBlock globalHeapBlocks[GLOBAL_HEAP_BLOCK_MAX];

void global_heap_init(EFIMemoryMap* memoryMap)
{
    tty_start_message("Global heap initializing");    

    memset(globalHeapBlocks, 0, sizeof(GlobalHeapBlock) * GLOBAL_HEAP_BLOCK_MAX);
    
    tty_end_message(TTY_MESSAGE_OK);
}

void global_heap_map(PageDirectory* pageDirectory)
{
    for (uint64_t i = 0; i < GLOBAL_HEAP_BLOCK_MAX; i++)
    {
        if (globalHeapBlocks[i].present)
        {
            page_directory_remap_pages(pageDirectory, 
            globalHeapBlocks[i].virtualStart, 
            globalHeapBlocks[i].physicalStart, 
            globalHeapBlocks[i].pageAmount, 
            globalHeapBlocks[i].pageFlags);
        }
    }
}

void* gmalloc(uint64_t pageAmount, uint16_t flags)
{    
    void* physicalStart = page_allocator_request_amount(pageAmount);

    void* virtualStart = (void*)((uint64_t)&_kernelStart - pageAmount * 0x1000);

    for (uint64_t i = 0; i < GLOBAL_HEAP_BLOCK_MAX; i++)
    {
        if (!globalHeapBlocks[i].present)
        {
            globalHeapBlocks[i].present = 1;
            globalHeapBlocks[i].virtualStart = virtualStart;
            globalHeapBlocks[i].physicalStart = physicalStart;
            globalHeapBlocks[i].pageAmount = pageAmount;
            globalHeapBlocks[i].pageFlags = flags;

            page_directory_remap_pages(kernelPageDirectory, virtualStart, physicalStart, pageAmount, flags);

            return virtualStart;
        }
        else
        {
            virtualStart = (void*)((uint64_t)virtualStart - globalHeapBlocks[i].pageAmount * 0x1000);
        }
    }

    debug_panic("No more global memory can be allocated!");
    return 0;
}

void* gfree(uint64_t pageAmount, uint16_t flags)
{
    debug_panic("gfree is unimplemented!");
    return 0;
}