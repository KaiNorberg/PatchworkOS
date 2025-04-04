#include "pmm.h"

#include "config.h"
#include "lock.h"
#include "log.h"
#include "smp.h"
#include "sys/proc.h"
#include "utils.h"
#include "vmm.h"

#include <bootloader/boot_info.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/math.h>

static uint64_t flexAreaSize;

static const char* efiMemTypeToString[] = {
    "reserved memory type",
    "loader code",
    "loader data",
    "boot services code",
    "boot services data",
    "runtime services code",
    "runtime services data",
    "conventional memory",
    "unusable memory",
    "acpi reclaim memory",
    "acpi memory nvs",
    "memory mapped io",
    "memory mapped io port space",
    "pal code",
    "persistent memory",
};

static page_stack_t stack;
static page_bitmap_t bitmap;

static uint64_t pageAmount = 0;
static uint64_t freePageAmount = 0;

static lock_t lock;

static void page_stack_init(void)
{
    stack.last = NULL;
    stack.index = 0;
}

static void* page_stack_alloc(void)
{
    void* address;
    if (stack.index == 0)
    {
        if (stack.last == NULL)
        {
            return NULL;
        }
        else
        {
            address = stack.last;
            stack.last = stack.last->prev;
            stack.index = PAGE_BUFFER_MAX - 1;
        }
    }
    else
    {
        address = stack.last->pages[--stack.index];
    }

    freePageAmount--;

    return address;
}

static void page_stack_free(void* address)
{
    freePageAmount++;

    if (stack.last == NULL)
    {
        stack.last = address;
        stack.last->prev = NULL;
        stack.index = 0;
    }
    else if (stack.index == PAGE_BUFFER_MAX)
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
}

static void page_bitmap_init(void)
{
    memset(bitmap.map, -1, sizeof(page_bitmap_t));
    bitmap.firstFreeIndex = 0;
}

static bool page_bitmap_reserved(uint64_t index)
{
    ASSERT_PANIC(index < PMM_MAX_SPECIAL_ADDR / PAGE_SIZE, "bitmap out of bounds");
    return (bitmap.map[(index) / 8] & (1ULL << ((index) % 8)));
}

static void page_bitmap_reserve(uint64_t low, uint64_t high)
{
    ASSERT_PANIC(high < PMM_MAX_SPECIAL_ADDR / PAGE_SIZE, "bitmap out of bounds");
    for (uint64_t i = low; i < high; i++)
    {
        bitmap.map[i / 8] |= (1ULL << (i % 8));
    }
    freePageAmount -= (high - low);
}

static void* page_bitmap_alloc(uint64_t count, uintptr_t maxAddr, uint64_t alignment)
{
    alignment = MAX(ROUND_UP(alignment, PAGE_SIZE), PAGE_SIZE);
    maxAddr = MIN(maxAddr, PMM_MAX_SPECIAL_ADDR);

    for (uint64_t i = bitmap.firstFreeIndex; i < maxAddr / PAGE_SIZE; i += alignment / PAGE_SIZE)
    {
        if (!page_bitmap_reserved(i))
        {
            uint64_t j = i + 1;
            for (; j < maxAddr / PAGE_SIZE; j++)
            {
                if (j - i == count)
                {
                    page_bitmap_reserve(i, j);
                    return (void*)(i * PAGE_SIZE + VMM_HIGHER_HALF_BASE);
                }

                if (page_bitmap_reserved(j))
                {
                    break;
                }
            }

            i = MAX(ROUND_UP(j, alignment / PAGE_SIZE), alignment / PAGE_SIZE) - alignment / PAGE_SIZE;
        }
    }

    return NULL;
}

static void page_bitmap_free(void* address)
{
    uint64_t index = ((uint64_t)address - VMM_HIGHER_HALF_BASE) / PAGE_SIZE;
    ASSERT_PANIC(index < PMM_MAX_SPECIAL_ADDR / PAGE_SIZE, "bitmap out of bounds");

    bitmap.map[index / 8] &= ~(1ULL << (index % 8));
    bitmap.firstFreeIndex = MIN(bitmap.firstFreeIndex, index);
    freePageAmount++;
}

static void pmm_free_unlocked(void* address)
{
    if ((uint64_t)address >= PMM_MAX_SPECIAL_ADDR + VMM_HIGHER_HALF_BASE)
    {
        page_stack_free(address);
    }
    else
    {
        page_bitmap_free(address);
    }
}

static void pmm_free_pages_unlocked(void* address, uint64_t count)
{
    for (uint64_t i = 0; i < count; i++)
    {
        pmm_free_unlocked((void*)((uint64_t)address + i * PAGE_SIZE));
    }
}

static void pmm_detect_memory(efi_mem_map_t* memoryMap)
{
    printf("UEFI-provided memory map");

    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);
        pageAmount += desc->amountOfPages;
    }

    printf("Detected memory: %d KB", (pageAmount * PAGE_SIZE) / 1024);
}

static void pmm_load_memory(efi_mem_map_t* memoryMap)
{
    for (uint64_t i = 0; i < memoryMap->descriptorAmount; i++)
    {
        const efi_mem_desc_t* desc = EFI_MEMORY_MAP_GET_DESCRIPTOR(memoryMap, i);

        if (EFI_IS_MEMORY_AVAIL(desc->type))
        {
            pmm_free_pages_unlocked(VMM_LOWER_TO_HIGHER(desc->physicalStart), desc->amountOfPages);
        }
    }
}

void pmm_init(efi_mem_map_t* memoryMap)
{
    lock_init(&lock);

    pmm_detect_memory(memoryMap);

    page_stack_init();
    page_bitmap_init();

    pmm_load_memory(memoryMap);
}

void* pmm_alloc(void)
{
    LOCK_DEFER(&lock);
    void* address = page_stack_alloc();
    return address != NULL ? address : ERRPTR(ENOMEM);
}

void* pmm_alloc_bitmap(uint64_t count, uintptr_t maxAddr, uint64_t alignment)
{
    LOCK_DEFER(&lock);
    void* address = page_bitmap_alloc(count, maxAddr, alignment);
    return address != NULL ? address : ERRPTR(ENOMEM);
}

void pmm_free(void* address)
{
    address = (void*)ROUND_DOWN(address, PAGE_SIZE);
    LOCK_DEFER(&lock);
    pmm_free_unlocked(address);
}

void pmm_free_pages(void* address, uint64_t count)
{
    address = (void*)ROUND_DOWN(address, PAGE_SIZE);
    LOCK_DEFER(&lock);
    pmm_free_pages_unlocked(address, count);
}

uint64_t pmm_total_amount(void)
{
    return pageAmount;
}

uint64_t pmm_free_amount(void)
{
    return freePageAmount;
}

uint64_t pmm_reserved_amount(void)
{
    return pageAmount - pmm_free_amount();
}
