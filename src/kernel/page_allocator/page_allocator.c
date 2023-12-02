#include "page_allocator.h"

#include "kernel/tty/tty.h"

#include "kernel/string/string.h"

extern uint64_t _kernelStart;
extern uint64_t _kernelEnd;

uint64_t* pageMap;

uint64_t pageAmount;
uint64_t lockedAmount;

void page_allocator_visualize()
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

    tty_print("Page allocator visualization:\n\r");

    uint64_t startOfSection = 0;

    uint8_t sectionStatus = page_allocator_get_status(0);
    for (uint64_t address = 0; address < pageAmount * 0x1000; address += 0x1000)
    {
        uint8_t addressStatus = page_allocator_get_status((void*)address);
        if (addressStatus != sectionStatus)
        {
            if (sectionStatus) //Reserved
            {
                tty_set_background(red);
            }
            else
            {
                tty_set_background(green);
            }

            tty_put(' '); tty_printx(startOfSection); tty_put('-'); tty_printx(address); tty_put(' '); 

            startOfSection = address;
            sectionStatus = addressStatus;
        }
    }                
    
    tty_set_background(black);
    tty_print("\n\n\r");
}

void page_allocator_init(EFIMemoryMap* memoryMap, Framebuffer* screenBuffer)
{    
    tty_start_message("Page allocator initializing");

    pageAmount = 0;
    for (uint64_t i = 0; i < memoryMap->DescriptorAmount; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->Base + (i * memoryMap->DescriptorSize));
        pageAmount += desc->AmountOfPages;
    }

    void* largestFreeSegment = 0;
    uint64_t largestFreeSegmentSize = 0;
    for (uint64_t i = 0; i < memoryMap->DescriptorAmount; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->Base + (i * memoryMap->DescriptorSize));
        
        if (desc->Type == EFI_CONVENTIONAL_MEMORY && largestFreeSegmentSize < desc->AmountOfPages * 0x1000)
        {
            largestFreeSegment = desc->PhysicalStart;
            largestFreeSegmentSize = desc->AmountOfPages * 0x1000;
        }
    }
    pageMap = largestFreeSegment;

    memset(pageMap, 0, pageAmount / 8);

    for (uint64_t i = 0; i < memoryMap->DescriptorAmount; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->Base + (i * memoryMap->DescriptorSize));

        if (is_memory_type_reserved(desc->Type))
        {
            page_allocator_lock_pages(desc->PhysicalStart, desc->AmountOfPages);
        }
    }

    page_allocator_lock_pages(pageMap, (pageAmount / 8) / 0x1000 + 1);
    page_allocator_lock_pages(&_kernelStart, ((uint64_t)&_kernelEnd - (uint64_t)&_kernelStart) / 0x1000 + 1);    
    page_allocator_lock_pages(screenBuffer->Base, screenBuffer->Size / 0x1000 + 1);

    tty_end_message(TTY_MESSAGE_OK);
}

uint8_t page_allocator_get_status(void* address)
{
    uint64_t index = (uint64_t)address / (uint64_t)0x1000;
    uint64_t qwordIndex = index / 64;
    uint64_t bitIndex = index % 64;

    return (pageMap[qwordIndex] & ((uint64_t)1 << bitIndex)) != 0;
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
                    void* address = (void*)((qwordIndex * 64 + bitIndex) * 0x1000);
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

void* page_allocator_request_amount(uint64_t amount)
{
    uint64_t startAddress = 0;
    uint64_t freePagesFound = 0;
    for (uint64_t address = 0; address < pageAmount * 0x1000; address += 0x1000)
    {
        if (page_allocator_get_status((void*)address)) //Reserved
        {
            startAddress = address;
            freePagesFound = 0;
        }
        else
        {
            freePagesFound++;
            page_allocator_lock_pages((void*)startAddress, freePagesFound);
            return (void*)startAddress;
        }
    }

    return 0;
}

void page_allocator_lock_page(void* address)
{
    uint64_t index = (uint64_t)address / 0x1000;
    pageMap[index / 64] = pageMap[index / 64] | (uint64_t)1 << (index % 64);
    lockedAmount++;
}

void page_allocator_unlock_page(void* address)
{
    uint64_t index = (uint64_t)address / 0x1000;
    pageMap[index / 64] = pageMap[index / 64] & ~((uint64_t)1 << (index % 64));
    lockedAmount--;    
}

void page_allocator_lock_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        page_allocator_lock_page((void*)((uint64_t)address + i * 0x1000));
    }
}

void page_allocator_unlock_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        page_allocator_unlock_page((void*)((uint64_t)address + i * 0x1000));
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