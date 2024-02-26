#include "pmm.h"

#include <libc/string.h>
#include <stddef.h>

#include "debug/debug.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "utils/utils.h"
#include <common/boot_info/boot_info.h>

static uint64_t* bitmap;
static uint64_t bitmapSize;
static void* firstFreePage;

static uint64_t pageAmount;
static uint64_t usablePageAmount;

static Lock lock;

static void pmm_allocate_bitmap(EfiMemoryMap* memoryMap)
{
    firstFreePage = 0;

    usablePageAmount = 0;
    uintptr_t highestAddress = 0;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        highestAddress = MAX(highestAddress, (uintptr_t)desc->physicalStart + desc->amountOfPages * PAGE_SIZE);        
        
        if (is_memory_type_usable(desc->type))
        {
            usablePageAmount += desc->amountOfPages;
        }
    }
    pageAmount = highestAddress / PAGE_SIZE;    

    bitmapSize = pageAmount / 8;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        
        if (!is_memory_type_reserved(desc->type) && bitmapSize < desc->amountOfPages * PAGE_SIZE)
        {
            bitmap = desc->virtualStart;    
            memset(bitmap, -1, bitmapSize);
            return;
        }
    }
}

static void pmm_load_memory_map(EfiMemoryMap* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));

        if (!is_memory_type_reserved(desc->type))
        {
            pmm_free_pages(desc->physicalStart, desc->amountOfPages);
        }
    }

    pmm_reserve_pages(vmm_virtual_to_physical(bitmap), SIZE_IN_PAGES(bitmapSize));
}

void pmm_init(EfiMemoryMap* memoryMap)
{    
    lock = lock_new();

    pmm_allocate_bitmap(memoryMap);

    pmm_load_memory_map(memoryMap);
}

void* pmm_allocate()
{
    lock_acquire(&lock);

    for (uint64_t qwordIndex = QWORD_INDEX(firstFreePage); qwordIndex < pageAmount / 64; qwordIndex++)
    {
        if (bitmap[qwordIndex] != UINT64_MAX) 
        {
            uint64_t bitIndex = (bitmap[qwordIndex] == 0) ? 0 : __builtin_ctzll(~bitmap[qwordIndex]);

            void* address = (void*)((qwordIndex * 64 + bitIndex) * PAGE_SIZE);
            pmm_reserve_page(address);

            lock_release(&lock);
            return address;
        }
    }

    debug_panic("Physical Memory Manager full!");

    lock_release(&lock);
    return 0;
}

void* pmm_allocate_amount(uint64_t amount)
{
    //TODO: Optimize this it sucks

    if (amount <= 1)
    {
        return pmm_allocate();
    }

    lock_acquire(&lock);

    uintptr_t startAddress = (uint64_t)-1;
    uint64_t freePagesFound = 0;
    for (uintptr_t address = 0; address < pageAmount * PAGE_SIZE; address += PAGE_SIZE)
    {
        if (pmm_is_reserved((void*)address))
        {
            startAddress = (uint64_t)-1;
        }
        else
        {
            if (startAddress == (uint64_t)-1)
            {
                startAddress = address;
                freePagesFound = 0;
            }

            freePagesFound++;
            if (freePagesFound == amount)
            {
                pmm_reserve_pages((void*)startAddress, freePagesFound);       

                lock_release(&lock);
                return (void*)startAddress;
            }
        }
    }
    
    debug_panic("Page allocator full!");

    lock_release(&lock);
    return 0;
}

uint8_t pmm_is_reserved(void* address)
{   
    return (bitmap[QWORD_INDEX(address)] >> BIT_INDEX(address)) & 1;
}

void pmm_reserve_page(void* address)
{        
    bitmap[QWORD_INDEX(address)] |= 1 << BIT_INDEX(address);

    if (firstFreePage == address)
    {
        firstFreePage = (void*)((uint64_t)firstFreePage + PAGE_SIZE);
    }
}

void pmm_free_page(void* address)
{
    bitmap[QWORD_INDEX(address)] &= ~(1 << BIT_INDEX(address));

    if (firstFreePage > address)
    {
        firstFreePage = address;
    }
}

void pmm_reserve_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        pmm_reserve_page((void*)((uint64_t)address + i * PAGE_SIZE));
    }
}

void pmm_free_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        pmm_free_page((void*)((uint64_t)address + i * PAGE_SIZE));
    }
}

uint64_t pmm_total_amount()
{
    return pageAmount;
}

uint64_t pmm_free_amount()
{
    return pageAmount - pmm_reserved_amount();
}

uint64_t pmm_reserved_amount()
{
    uint64_t amount = 0;
    for (uint64_t i = 0; i < pageAmount; ++i) 
    {
        amount += pmm_is_reserved((void*)(i * PAGE_SIZE));
    }
    return amount;
}

uint64_t pmm_usable_amount()
{
    return usablePageAmount;
}

uint64_t pmm_unusable_amount()
{
    return pageAmount - usablePageAmount;
}