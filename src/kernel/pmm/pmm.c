#include "pmm.h"

#include "tty/tty.h"
#include "debug/debug.h"
#include "lock/lock.h"
#include "vmm/vmm.h"
#include "utils/utils.h"

#include <libc/string.h>

static uintptr_t physicalBase;

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
            bitmap = desc->physicalStart;    
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
            pmm_unlock_pages(desc->physicalStart, desc->amountOfPages);
        }
    }

    pmm_lock_pages(bitmap, SIZE_IN_PAGES(bitmapSize));
}

void pmm_init(EfiMemoryMap* memoryMap)
{    
    physicalBase = 0;
    lock = lock_new();

    pmm_allocate_bitmap(memoryMap);

    pmm_load_memory_map(memoryMap);
}

void* pmm_physical_base()
{
    return (void*)physicalBase;
}

void pmm_move_to_higher_half()
{
    physicalBase = VMM_PHYSICAL_BASE;
    bitmap = vmm_physical_to_virtual(bitmap);
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
            pmm_lock_page(address);

            lock_release(&lock);
            return address;
        }
    }

    debug_panic("Physical Memory Manager full!");

    lock_release(&lock);
    return NULL;
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
        if (pmm_is_locked((void*)address))
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
                pmm_lock_pages((void*)startAddress, freePagesFound);       

                lock_release(&lock);
                return (void*)startAddress;
            }
        }
    }
    
    debug_panic("Page allocator full!");

    lock_release(&lock);
    return 0;
}

uint8_t pmm_is_locked(void* address)
{   
    return (bitmap[QWORD_INDEX(address)] >> BIT_INDEX(address)) & 1;
}

void pmm_lock_page(void* address)
{        
    bitmap[QWORD_INDEX(address)] |= 1 << BIT_INDEX(address);

    if (firstFreePage == address)
    {
        firstFreePage = (void*)((uint64_t)firstFreePage + PAGE_SIZE);
    }
}

void pmm_unlock_page(void* address)
{
    bitmap[QWORD_INDEX(address)] &= ~(1 << BIT_INDEX(address));

    if (firstFreePage > address)
    {
        firstFreePage = address;
    }
}

void pmm_lock_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        pmm_lock_page((void*)((uint64_t)address + i * PAGE_SIZE));
    }
}

void pmm_unlock_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        pmm_unlock_page((void*)((uint64_t)address + i * PAGE_SIZE));
    }
}

uint64_t pmm_unlocked_amount()
{
    return pageAmount - pmm_locked_amount();
}

uint64_t pmm_locked_amount()
{
    uint64_t amount = 0;
    for (uint64_t i = 0; i < pageAmount; ++i) 
    {
        amount += pmm_is_locked((void*)(i * PAGE_SIZE));
    }
    return amount;
}

uint64_t pmm_total_amount()
{
    return pageAmount;
}

uint64_t pmm_usable_amount()
{
    return usablePageAmount;
}

uint64_t pmm_unusable_amount()
{
    return pageAmount - usablePageAmount;
}