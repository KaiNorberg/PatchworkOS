#include "global_heap.h"

#include "page_allocator/page_allocator.h"
#include "string/string.h"
#include "tty/tty.h"
#include "debug/debug.h"
#include "utils/utils.h"
#include "lock/lock.h"

#include "../common.h"

extern uint64_t _kernelStart;

uintptr_t globalHeapTop;
uintptr_t globalHeapBottom;

Lock globalHeapLock;

void global_heap_init()
{
    tty_start_message("Global heap initializing");    

    globalHeapTop = round_down((uint64_t)&_kernelStart, 0x1000);
    globalHeapBottom = globalHeapTop;

    globalHeapLock = lock_new();

    tty_end_message(TTY_MESSAGE_OK);
}

void global_heap_map(PageDirectory* pageDirectory)
{
    uint64_t pageAmount = (globalHeapTop - globalHeapBottom) / 0x1000;

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        void* virtualAddress = (void*)(globalHeapBottom + i * 0x1000);
        void* physicalAddress = page_directory_get_physical_address(kernelPageDirectory, virtualAddress);

        page_directory_remap(pageDirectory, virtualAddress, physicalAddress, PAGE_DIR_READ_WRITE);
    }
}

void* gmalloc(uint64_t pageAmount)
{   
    lock_acquire(&globalHeapLock); 

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        globalHeapBottom -= 0x1000;

        void* physicalAddress = page_allocator_request();

        page_directory_remap(kernelPageDirectory, (void*)globalHeapBottom, physicalAddress, PAGE_DIR_READ_WRITE);
    }

    lock_release(&globalHeapLock); 

    return (void*)globalHeapBottom;
}