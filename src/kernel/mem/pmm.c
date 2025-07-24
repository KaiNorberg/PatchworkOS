#include "pmm.h"

#include "config.h"
#include "cpu/smp.h"
#include "gnu-efi/inc/efidef.h"
#include "log/log.h"
#include "log/panic.h"
#include "sched/thread.h"
#include "sync/lock.h"
#include "sys/proc.h"
#include "utils/bitmap.h"
#include "utils/utils.h"
#include "vmm.h"

#include <boot/boot_info.h>

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <sys/math.h>

static const char* efiMemTypeToString[] = {
    "reserved",
    "loader code",
    "loader data",
    "boot services code",
    "boot services data",
    "runtime services code",
    "runtime services data",
    "conventional",
    "unusable",
    "acpi reclaim",
    "acpi memory nvs",
    "io",
    "io port space",
    "pal code",
    "persistent",
};

// Stores the bitmap data
static uint64_t mapBuffer[BITMAP_BITS_TO_QWORDS(PMM_BITMAP_MAX)];

static pmm_stack_t stack;
static bitmap_t bitmap;

static uint64_t pageAmount = 0;
static uint64_t freePageAmount = 0;

static lock_t lock = LOCK_CREATE();

static bool pmm_is_efi_mem_available(EFI_MEMORY_TYPE type)
{
    switch (type)
    {
    case EfiConventionalMemory:
    case EfiLoaderCode:
    // EfiLoaderData is intentionally not included here as it's freed later in `kernel_init()`.
    case EfiBootServicesCode:
    case EfiBootServicesData:
        return true;
    default:
        return false;
    }
}

static bool pmm_is_efi_mem_ram(EFI_MEMORY_TYPE type)
{
    switch (type)
    {
    case EfiConventionalMemory:
    case EfiLoaderCode:
    case EfiLoaderData:
    case EfiBootServicesCode:
    case EfiBootServicesData:
    case EfiRuntimeServicesCode:
    case EfiRuntimeServicesData:
    case EfiACPIReclaimMemory:
    case EfiACPIMemoryNVS:
        return true;
    default:
        return false;
    }
}

static void pmm_stack_init(void)
{
    stack.last = NULL;
    stack.index = 0;
}

static void* pmm_stack_alloc(void)
{
    if (stack.last == NULL)
    {
        return NULL;
    }

    void* address;
    if (stack.index == 0)
    {
        address = stack.last;
        stack.last = stack.last->prev;
        stack.index = PMM_BUFFER_MAX - 1;
    }
    else
    {
        address = stack.last->pages[--stack.index];
    }

    freePageAmount--;

    return address;
}

static void pmm_stack_free(void* address)
{
    if (stack.last == NULL)
    {
        stack.last = address;
        stack.last->prev = NULL;
        stack.index = 0;
    }
    else if (stack.index == PMM_BUFFER_MAX)
    {
        page_buffer_t* next = address;
        next->prev = stack.last;
        stack.last = next;
        stack.index = 0;
    }
    else
    {
        stack.last->pages[stack.index++] = address;
    }

    freePageAmount++;
}

static void pmm_bitmap_init(void)
{
    bitmap_init(&bitmap, mapBuffer, PMM_BITMAP_MAX);
    memset(mapBuffer, -1, BITMAP_BITS_TO_BYTES(PMM_BITMAP_MAX));
}

static bool pmm_bitmap_is_reserved(uint64_t index)
{
    assert(index < PMM_BITMAP_MAX_ADDR / PAGE_SIZE);

    return bitmap_is_set(&bitmap, index);
}

static void pmm_bitmap_reserve(uint64_t low, uint64_t high)
{
    assert(low <= high);
    assert(high < PMM_BITMAP_MAX_ADDR / PAGE_SIZE);

    bitmap_set_range(&bitmap, low, high);
    freePageAmount -= (high - low);
}

static void* pmm_bitmap_alloc(uint64_t count, uintptr_t maxAddr, uint64_t alignment)
{
    alignment = MAX(ROUND_UP(alignment, PAGE_SIZE), PAGE_SIZE);
    maxAddr = MIN(maxAddr, PMM_BITMAP_MAX_ADDR);

    uint64_t index = bitmap_find_clear_region_and_set(&bitmap, count, maxAddr / PAGE_SIZE, alignment / PAGE_SIZE);
    if (index == ERR)
    {
        return NULL;
    }

    return (void*)(index * PAGE_SIZE + PML_HIGHER_HALF_START);
}

static void pmm_bitmap_free(void* address)
{
    uint64_t index = (uint64_t)PML_HIGHER_TO_LOWER(address) / PAGE_SIZE;
    assert(index < PMM_BITMAP_MAX_ADDR / PAGE_SIZE);

    bitmap_clear(&bitmap, index);
    freePageAmount++;
}

static void pmm_free_unlocked(void* address)
{
    if (address >= PML_LOWER_TO_HIGHER(PMM_BITMAP_MAX_ADDR))
    {
        pmm_stack_free(address);
    }
    else if (address >= PML_LOWER_TO_HIGHER(0))
    {
        pmm_bitmap_free(address);
    }
    else
    {
        panic(NULL, "pmm: attempt to free lower half address %p", address);
    }
}

static void pmm_free_pages_unlocked(void* address, uint64_t count)
{
    uint64_t physStart = (uint64_t)PML_HIGHER_TO_LOWER(address);
    uint64_t physEnd = physStart + count * PAGE_SIZE;

    if (physEnd <= PMM_BITMAP_MAX_ADDR)
    {
        uint64_t startIndex = physStart / PAGE_SIZE;

        bitmap_clear_range(&bitmap, startIndex, startIndex + count);
        freePageAmount += count;
    }
    else if (physStart < PMM_BITMAP_MAX_ADDR)
    {
        uint64_t bitmapPageCount = (PMM_BITMAP_MAX_ADDR - physStart) / PAGE_SIZE;
        uint64_t startIndex = physStart / PAGE_SIZE;

        bitmap_clear_range(&bitmap, startIndex, startIndex + bitmapPageCount);
        freePageAmount += bitmapPageCount;

        void* stackAddr = PML_LOWER_TO_HIGHER(PMM_BITMAP_MAX_ADDR);
        for (uint64_t i = 0; i < count - bitmapPageCount; i++)
        {
            pmm_stack_free((void*)((uint64_t)stackAddr + i * PAGE_SIZE));
        }
    }
    else
    {
        for (uint64_t i = 0; i < count; i++)
        {
            pmm_stack_free((void*)((uint64_t)address + i * PAGE_SIZE));
        }
    }
}

static void pmm_detect_memory(boot_memory_map_t* memoryMap)
{
    LOG_INFO("UEFI-provided memory map\n");

    for (uint64_t i = 0; i < memoryMap->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (pmm_is_efi_mem_ram(desc->Type))
        {
            pageAmount += desc->NumberOfPages;
        }
    }
}

static void pmm_load_memory(boot_memory_map_t* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (pmm_is_efi_mem_available(desc->Type))
        {
            pmm_free_pages_unlocked(PML_LOWER_TO_HIGHER(desc->PhysicalStart), desc->NumberOfPages);
        }
        else
        {
            LOG_INFO("reserve [0x%016lx-0x%016lx] pages=%d type=%s\n", desc->PhysicalStart,
                (uint64_t)desc->PhysicalStart + desc->NumberOfPages * PAGE_SIZE, desc->NumberOfPages,
                efiMemTypeToString[desc->Type]);
        }
    }

    LOG_INFO("memory %llu MB (usable %llu MB reserved %llu MB)\n", (pageAmount * PAGE_SIZE) / 1000000,
        (freePageAmount * PAGE_SIZE) / 1000000, ((pageAmount - freePageAmount) * PAGE_SIZE) / 1000000);
}

void pmm_init(boot_memory_map_t* memoryMap)
{
    pmm_detect_memory(memoryMap);

    pmm_stack_init();
    pmm_bitmap_init();

    pmm_load_memory(memoryMap);
}

void* pmm_alloc(void)
{
    LOCK_SCOPE(&lock);
    void* address = pmm_stack_alloc();
    if (address == NULL)
    {
        LOG_WARN("failed to allocate single page, free=%llu pages\n", freePageAmount);
        errno = ENOMEM;
        return NULL;
    }
    return address;
}

void* pmm_alloc_bitmap(uint64_t count, uintptr_t maxAddr, uint64_t alignment)
{
    LOCK_SCOPE(&lock);
    void* address = pmm_bitmap_alloc(count, maxAddr, alignment);
    if (address == NULL)
    {
        LOG_WARN("failed to allocate %llu pages from bitmap, free=%llu pages, maxAddr=0x%lx\n", count, freePageAmount,
            maxAddr);
        errno = ENOMEM;
        return NULL;
    }
    return address;
}

void pmm_free(void* address)
{
    address = (void*)ROUND_DOWN(address, PAGE_SIZE);
    LOCK_SCOPE(&lock);
    pmm_free_unlocked(address);
}

void pmm_free_pages(void* address, uint64_t count)
{
    address = (void*)ROUND_DOWN(address, PAGE_SIZE);
    LOCK_SCOPE(&lock);
    pmm_free_pages_unlocked(address, count);
}

uint64_t pmm_total_amount(void)
{
    LOCK_SCOPE(&lock);
    return pageAmount;
}

uint64_t pmm_free_amount(void)
{
    LOCK_SCOPE(&lock);
    return freePageAmount;
}

uint64_t pmm_reserved_amount(void)
{
    LOCK_SCOPE(&lock);
    return pageAmount - freePageAmount;
}
