#include "pmm.h"

#include "tty/tty.h"
#include "debug/debug.h"
#include "lock/lock.h"
#include "vmm/vmm.h"

#include <libc/string.h>

static uintptr_t physicalBase;

static uint64_t* bitmap;
static uint64_t totalAmount;
static uint64_t lockedAmount;
static void* firstFreePage;

static Lock lock;

static void pmm_bitmap_init(EfiMemoryMap* memoryMap)
{
    totalAmount = 0;
    lockedAmount = 0;
    firstFreePage = 0;

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        totalAmount += desc->amountOfPages;
    }
    uint64_t bitmapSize = totalAmount / 8;

    bitmap = 0;
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));
        
        if (desc->physicalStart > (void*)0x8000 && desc->type == EFI_CONVENTIONAL_MEMORY && bitmapSize < desc->amountOfPages * 0x1000)
        {
            bitmap = desc->physicalStart;
            memset(bitmap, 0, bitmapSize);
            break;
        }
    }    
    
    pmm_lock_pages(bitmap, SIZE_IN_PAGES(bitmapSize));
}

static void pmm_load_memory_map(EfiMemoryMap* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const EfiMemoryDescriptor* desc = (EfiMemoryDescriptor*)((uint64_t)memoryMap->base + (i * memoryMap->descriptorSize));

        if (is_memory_type_reserved(desc->type) && ((uint64_t)desc->physicalStart) < totalAmount * 0x1000)
        {
            pmm_lock_pages(desc->physicalStart, desc->amountOfPages);
        }
    }
}

void pmm_init(EfiMemoryMap* memoryMap)
{    
    physicalBase = 0;
    lock = lock_new();

    pmm_bitmap_init(memoryMap);

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

    uint64_t firstFreeQwordIndex = ((uint64_t)firstFreePage / 0x1000) / 64;
    for (uint64_t qwordIndex = firstFreeQwordIndex; qwordIndex < totalAmount / 64; qwordIndex++)
    {        
        if (bitmap[qwordIndex] != (uint64_t)-1) //If any bit is zero
        {            
            for (uint64_t bitIndex = 0; bitIndex < 64; bitIndex++)
            {
                if (((bitmap[qwordIndex] >> bitIndex) & 1) == 0) //If bit is not set
                {
                    void* address = (void*)((qwordIndex * 64 + bitIndex) * 0x1000);
                    pmm_lock_page(address);

                    lock_release(&lock);
                    return (void*)address;
                }
            }

            debug_panic("Physical Memory Manager confused!");
        }
    }

    debug_panic("Physical Memory Manager full!");

    lock_release(&lock);
    return 0;
}

void* pmm_allocate_amount(uint64_t amount)
{
    //TODO: Optmize this it sucks

    if (amount <= 1)
    {
        return pmm_allocate();
    }
    lock_acquire(&lock);

    uintptr_t startAddress = (uint64_t)-1;
    uint64_t freePagesFound = 0;
    for (uintptr_t address = 0; address < totalAmount * 0x1000; address += 0x1000)
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

uint8_t pmm_is_reserved(void* address)
{   
    uint64_t index = (uint64_t)address / (uint64_t)0x1000;
    return (bitmap[index / 64] >> (index % 64)) & 1;
}

void pmm_lock_page(void* address)
{        
    if (!pmm_is_reserved(address))
    {
        uint64_t index = (uint64_t)address / (uint64_t)0x1000;

        bitmap[index / 64] |= 1 << (index % 64);

        lockedAmount++;

        if (firstFreePage == address)
        {
            firstFreePage = (void*)((uint64_t)firstFreePage + 0x1000);
        }
    }
}

void pmm_unlock_page(void* address)
{
    if (pmm_is_reserved(address))
    {
        uint64_t index = (uint64_t)address / (uint64_t)0x1000;

        bitmap[index / 64] &= ~(1 << (index % 64));

        lockedAmount--;

        if (firstFreePage > address)
        {
            firstFreePage = address;
        }
    }
}

void pmm_lock_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        pmm_lock_page((void*)((uint64_t)address + i * 0x1000));
    }
}

void pmm_unlock_pages(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        pmm_unlock_page((void*)((uint64_t)address + i * 0x1000));
    }
}

uint64_t pmm_unlocked_amount()
{
    return totalAmount - lockedAmount;
}

uint64_t pmm_locked_amount()
{
    return lockedAmount;
}

uint64_t pmm_total_amount()
{
    return totalAmount;
}