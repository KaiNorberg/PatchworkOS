#include "page_allocator.h"

#include "tty/tty.h"
#include "string/string.h"
#include "debug/debug.h"
#include "lock/lock.h"

static uint64_t* pageMap;
static void* firstFreePage;

static uint64_t pageAmount;
static uint64_t lockedAmount;

static Lock lock;

void page_allocator_visualize()
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

    tty_print("Page allocator visualization:\n\r");

    uint64_t startOfSection = 0;

    uint8_t sectionStatus = page_allocator_is_reserved(0);
    for (uint64_t address = 0; address < pageAmount * 0x1000; address += 0x1000)
    {
        uint8_t addressStatus = page_allocator_is_reserved((void*)address);
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

    if (sectionStatus) //Reserved
    {
        tty_set_background(red);
    }
    else
    {
        tty_set_background(green);
    }
    tty_put(' '); tty_printx(startOfSection); tty_put('-'); tty_printx(pageAmount * 0x1000); tty_put(' '); 

    tty_set_background(black);
    tty_print("\n\n\r");
}

void page_allocator_init(EfiMemoryMap* memoryMap)
{    
    tty_start_message("Page allocator initializing");

    lock = lock_new();

    lockedAmount = 0;

    firstFreePage = 0;

    pageAmount = 0;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        pageAmount += desc->amountOfPages;
    }
    uint64_t pageMapSize = pageAmount / 8;

    pageMap = 0;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        
        if (desc->physicalStart >= (void*)0x9000 && desc->type == EFI_CONVENTIONAL_MEMORY && pageMapSize < desc->amountOfPages * 0x1000)
        {
            pageMap = desc->physicalStart;
            memset(pageMap, 0, pageMapSize);
            break;
        }
    } 
    if (pageMap == 0)
    {
        tty_print("Unable to find suitable location for the page map");    
        tty_end_message(TTY_MESSAGE_ER);
    }

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        EFIMemoryDescriptor* desc = (EFIMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));

        if (is_memory_type_reserved(desc->type) && ((uint64_t)desc->physicalStart) < pageAmount * 0x1000)
        {
            page_allocator_lock_pages(desc->physicalStart, desc->amountOfPages);
        }
    }

    page_allocator_lock_pages(pageMap, GET_SIZE_IN_PAGES(pageMapSize));

    tty_end_message(TTY_MESSAGE_OK);
}

void* page_allocator_request()
{   
    lock_acquire(&lock);

    uint64_t firstFreeQwordIndex = ((uint64_t)firstFreePage / 0x1000) / 64;
    for (uint64_t qwordIndex = firstFreeQwordIndex; qwordIndex < pageAmount / 64; qwordIndex++)
    {        
        if (pageMap[qwordIndex] != (uint64_t)-1) //If any bit is zero
        {            
            for (uint64_t bitIndex = 0; bitIndex < 64; bitIndex++)
            {
                if (((pageMap[qwordIndex] >> bitIndex) & 1) == 0) //If bit is not set
                {
                    void* address = (void*)((qwordIndex * 64 + bitIndex) * 0x1000);
                    page_allocator_lock_page(address);

                    lock_release(&lock);
                    return address;
                }
            }

            debug_panic("Page allocator confused!");
        }
    }

    debug_panic("Page allocator full!");

    lock_release(&lock);
    return 0;
}

void* page_allocator_request_amount(uint64_t amount)
{
    if (amount <= 1)
    {
        return page_allocator_request();
    } 
    lock_acquire(&lock);

    uint64_t startAddress = 0;
    uint64_t freePagesFound = 0;
    for (uint64_t address = 0; address < pageAmount * 0x1000; address += 0x1000)
    {
        if (page_allocator_is_reserved((void*)address)) //Reserved
        {
            startAddress = address;
            freePagesFound = 0;
        }
        else
        {
            freePagesFound++;
            if (freePagesFound == amount)
            {
                page_allocator_lock_pages((void*)startAddress, freePagesFound);       

                lock_release(&lock);
                return (void*)startAddress;                
            }
        }
    }
    
    debug_panic("Page allocator full!");

    lock_release(&lock);
    return 0;
}

uint8_t page_allocator_is_reserved(void* address)
{   
    uint64_t index = (uint64_t)address / (uint64_t)0x1000;
    return (pageMap[index / 64] >> (index % 64)) & 1;
}

void page_allocator_lock_page(void* address)
{        
    if (!page_allocator_is_reserved(address))
    {
        uint64_t index = (uint64_t)address / (uint64_t)0x1000;

        pageMap[index / 64] |= 1 << (index % 64);

        lockedAmount++;

        if (firstFreePage == address)
        {
            firstFreePage = (void*)((uint64_t)firstFreePage + 0x1000);
        }
    }
}

void page_allocator_unlock_page(void* address)
{
    if (page_allocator_is_reserved(address))
    {
        uint64_t index = (uint64_t)address / (uint64_t)0x1000;

        pageMap[index / 64] &= ~(1 << (index % 64));

        lockedAmount--;

        if (firstFreePage > address)
        {
            firstFreePage = address;
        }
    }
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

uint64_t page_allocator_unlocked_amount()
{
    return pageAmount - lockedAmount;
}

uint64_t page_allocator_locked_amount()
{
    return lockedAmount;
}

uint64_t page_allocator_get_total_amount()
{
    return pageAmount;
}