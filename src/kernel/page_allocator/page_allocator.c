#include "page_allocator.h"

#include "kernel/tty/tty.h"

#include "libc/include/string.h"

extern uint64_t _kernelStart;
extern uint64_t _kernelEnd;

uint64_t* pageMap;

uint64_t pageAmount;
uint64_t lockedAmount;

void page_allocator_init(EFIMemoryMap* memoryMap, Framebuffer* screenBuffer)
{
    pageAmount = 0;
    for (uint64_t i = 0; i < memoryMap->Size / memoryMap->DescriptorSize; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->Base + (i * memoryMap->DescriptorSize));
        pageAmount += desc->AmountOfPages;
    }

    void* largestFreeSegment = 0;
    uint64_t largestFreeSegmentSize = 0;
    for (uint64_t i = 0; i < memoryMap->Size / memoryMap->DescriptorSize; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->Base + (i * memoryMap->DescriptorSize));
        
        if (desc->Type == EFI_CONVENTIONAL_MEMORY && largestFreeSegmentSize < desc->AmountOfPages * 4096)
        {
            largestFreeSegment = desc->PhysicalStart;
            largestFreeSegmentSize = desc->AmountOfPages * 4096;
        }
    }
    pageMap = largestFreeSegment;

    memset(pageMap, 0, pageAmount / 8);

    for (uint64_t i = 0; i < memoryMap->Size / memoryMap->DescriptorSize; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->Base + (i * memoryMap->DescriptorSize));

        if (desc->Type != EFI_CONVENTIONAL_MEMORY)
        {
            page_allocator_lock_pages(desc->PhysicalStart, desc->AmountOfPages);
        }
    }

    page_allocator_lock_pages(pageMap, (pageAmount / 8) / 4096 + 1);
    page_allocator_lock_pages(&_kernelStart, ((uint64_t)&_kernelEnd - (uint64_t)&_kernelStart) / 4096 + 1);    
    page_allocator_lock_pages(screenBuffer->Base, screenBuffer->Size / 4096 + 1);
}

uint8_t page_allocator_get_status(void* address)
{
    uint64_t index = (uint64_t)address / 4096;
    uint64_t qwordIndex = index / 64;
    uint64_t bitIndex = index % 64;

    return pageMap[qwordIndex] & ((uint64_t)1 << bitIndex);
}

void* page_allocator_request()
{
    for (uint64_t qwordIndex = 0; qwordIndex < pageAmount; qwordIndex++)
    {
        if (pageMap[qwordIndex] != 0xFFFFFFFFFFFFFFFF) //If any bit is zero
        {            
            for (uint64_t bitIndex = 0; bitIndex < 64; bitIndex++)
            {
                if ((pageMap[qwordIndex] & ((uint64_t)1 << bitIndex)) == 0) //If bit is not set
                {
                    void* address = (void*)((qwordIndex * 64 + bitIndex) * 4096);
                    page_allocator_lock_page(address);
                    return address;
                }
            }

            tty_print("ERROR: Page allocator confused!");
        }
    }

    tty_print("ERROR: Page allocator full!");

    return 0;
}

void page_allocator_lock_page(void* address)
{
    uint64_t index = (uint64_t)address / 4096;
    pageMap[index / 64] = pageMap[index / 64] | (uint64_t)1 << (index % 64);
    lockedAmount++;
}

void page_allocator_unlock_page(void* address)
{
    uint64_t index = (uint64_t)address / 4096;
    pageMap[index / 64] = pageMap[index / 64] & ~((uint64_t)1 << (index % 64));
    lockedAmount--;    
}

void page_allocator_lock_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        page_allocator_lock_page((void*)((uint64_t)address + i * 4096));
    }
}

void page_allocator_unlock_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        page_allocator_unlock_page((void*)((uint64_t)address + i * 4096));
    }
}

uint64_t page_allocator_get_unlocked_amount()
{
    return pageAmount - lockedAmount;
}

uint64_t page_allocator_get_locked_amount()
{
    return lockedAmount;
}

uint64_t page_allocator_get_total_amount()
{
    return pageAmount;
}