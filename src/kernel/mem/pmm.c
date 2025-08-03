#include "pmm.h"

#include "config.h"
#include "log/log.h"
#include "log/panic.h"
#include "mem/pmm_bitmap.h"
#include "mem/pmm_stack.h"
#include "sync/lock.h"
#include "sys/proc.h"
#include "utils/bitmap.h"

#include <boot/boot_info.h>

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

#define PMM_BITMAP_SIZE (CONFIG_PMM_BITMAP_MAX_ADDR / PAGE_SIZE)

static pmm_stack_t stack;
static pmm_bitmap_t bitmap;

// Stores the bitmap data
static uint64_t mapBuffer[BITMAP_BITS_TO_QWORDS(PMM_BITMAP_SIZE)];

static uint64_t pageAmount = 0;

static lock_t lock = LOCK_CREATE();

static bool pmm_is_efi_mem_available(EFI_MEMORY_TYPE type)
{
    if (!boot_is_mem_ram(type))
    {
        return false;
    }

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

static void pmm_free_unlocked(void* address)
{
    if (address >= PML_LOWER_TO_HIGHER(CONFIG_PMM_BITMAP_MAX_ADDR))
    {
        pmm_stack_free(&stack, address);
    }
    else if (address >= PML_LOWER_TO_HIGHER(0))
    {
        pmm_bitmap_free(&bitmap, address, 1);
    }
    else
    {
        panic(NULL, "pmm: attempt to free lower half address %p", address);
    }
}

static void pmm_free_pages_unlocked(void* address, uint64_t count)
{
    address = (void*)ROUND_DOWN(address, PAGE_SIZE);

    uint64_t physStart = (uint64_t)PML_HIGHER_TO_LOWER(address);
    uint64_t physEnd = physStart + count * PAGE_SIZE;

    if (physEnd <= CONFIG_PMM_BITMAP_MAX_ADDR)
    {
        pmm_bitmap_free(&bitmap, address, count);
    }
    else if (physStart < CONFIG_PMM_BITMAP_MAX_ADDR)
    {
        uint64_t bitmapPageCount = (CONFIG_PMM_BITMAP_MAX_ADDR - physStart) / PAGE_SIZE;
        pmm_bitmap_free(&bitmap, address, bitmapPageCount);

        void* stackAddr = PML_LOWER_TO_HIGHER(CONFIG_PMM_BITMAP_MAX_ADDR);
        for (uint64_t i = 0; i < count - bitmapPageCount; i++)
        {
            pmm_stack_free(&stack, (void*)((uint64_t)stackAddr + i * PAGE_SIZE));
        }
    }
    else
    {
        for (uint64_t i = 0; i < count; i++)
        {
            pmm_stack_free(&stack, (void*)((uint64_t)address + i * PAGE_SIZE));
        }
    }
}

static void pmm_detect_memory(boot_memory_map_t* map)
{
    LOG_INFO("UEFI-provided memory map\n");

    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (boot_is_mem_ram(desc->Type))
        {
            pageAmount += desc->NumberOfPages;
        }
    }

    LOG_INFO("page amount %llu\n", pageAmount);
}

static void pmm_load_memory(boot_memory_map_t* map)
{
    for (uint64_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (pmm_is_efi_mem_available(desc->Type))
        {
            pmm_free_pages_unlocked((void*)desc->VirtualStart, desc->NumberOfPages);
        }
        else
        {
            LOG_INFO("reserve [0x%016lx-0x%016lx] pages=%d type=%s\n", desc->VirtualStart,
                (uint64_t)desc->VirtualStart + desc->NumberOfPages * PAGE_SIZE, desc->NumberOfPages,
                efiMemTypeToString[desc->Type]);
        }
    }

    LOG_INFO("memory %llu MB (usable %llu MB reserved %llu MB)\n", (pageAmount * PAGE_SIZE) / 1000000,
        (pmm_free_amount() * PAGE_SIZE) / 1000000, ((pageAmount - pmm_free_amount()) * PAGE_SIZE) / 1000000);
}

void pmm_init(boot_memory_map_t* map)
{
    pmm_detect_memory(map);

    pmm_stack_init(&stack);
    pmm_bitmap_init(&bitmap, mapBuffer, PMM_BITMAP_SIZE, CONFIG_PMM_BITMAP_MAX_ADDR);

    pmm_load_memory(map);
}

void* pmm_alloc(void)
{
    LOCK_SCOPE(&lock);
    void* address = pmm_stack_alloc(&stack);
    if (address == NULL)
    {
        LOG_WARN("failed to allocate single page, free=%llu pages\n", pmm_free_amount());
        errno = ENOMEM;
        return NULL;
    }
    return address;
}

void* pmm_alloc_bitmap(uint64_t count, uintptr_t maxAddr, uint64_t alignment)
{
    LOCK_SCOPE(&lock);
    void* address = pmm_bitmap_alloc(&bitmap, count, maxAddr, alignment);
    if (address == NULL)
    {
        LOG_WARN("failed to allocate %llu pages from bitmap, free=%llu pages, maxAddr=0x%lx\n", count,
            pmm_free_amount(), maxAddr);
        errno = ENOMEM;
        return NULL;
    }
    return address;
}

void pmm_free(void* address)
{
    LOCK_SCOPE(&lock);
    pmm_free_unlocked(address);
}

void pmm_free_pages(void* address, uint64_t count)
{
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
    return stack.free + bitmap.free;
}

uint64_t pmm_reserved_amount(void)
{
    LOCK_SCOPE(&lock);
    return pageAmount - (stack.free + bitmap.free);
}
