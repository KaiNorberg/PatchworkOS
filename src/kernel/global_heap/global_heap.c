#include "global_heap.h"

#include "page_allocator/page_allocator.h"
#include "tty/tty.h"
#include "debug/debug.h"
#include "utils/utils.h"
#include "lock/lock.h"

#include <common/common.h>

extern uint64_t _kernelStart;

static uintptr_t top;
static uintptr_t bottom;

static Lock lock;

void global_heap_init()
{
    tty_start_message("Global heap initializing");    

    top = round_down((uint64_t)&_kernelStart, 0x1000);
    bottom = top;

    lock = lock_new();

    tty_end_message(TTY_MESSAGE_OK);
}

void global_heap_map(PageDirectory* pageDirectory)
{
    uint64_t pageAmount = (top - bottom) / 0x1000;

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        void* virtualAddress = (void*)(bottom + i * 0x1000);
        void* physicalAddress = page_directory_get_physical_address(kernelPageDirectory, virtualAddress);

        page_directory_remap(pageDirectory, virtualAddress, physicalAddress, PAGE_DIR_READ_WRITE);
    }
}

void* gmalloc(uint64_t pageAmount)
{   
    lock_acquire(&lock); 

    for (uint64_t i = 0; i < pageAmount; i++)
    {
        bottom -= 0x1000;

        void* physicalAddress = page_allocator_request();

        page_directory_remap(kernelPageDirectory, (void*)bottom, physicalAddress, PAGE_DIR_READ_WRITE);
    }

    lock_release(&lock); 

    return (void*)bottom;
}