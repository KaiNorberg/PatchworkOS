#include <kernel/mem/paging_types.h>
#include <kernel/mem/pmm.h>

#include <kernel/config.h>
#include <kernel/init/boot_info.h>
#include <kernel/log/log.h>
#include <kernel/log/panic.h>
#include <kernel/sync/lock.h>

#include <boot/boot_info.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/bitmap.h>

#include <errno.h>
#include <string.h>
#include <sys/math.h>
#include <sys/proc.h>

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

static page_t* pages = NULL;

static page_stack_t* stack = NULL;
static size_t location = FREE_PAGE_MAX;

BITMAP_CREATE_ONE(bitmap, CONFIG_PMM_BITMAP_MAX_ADDR / PAGE_SIZE);

static pfn_t highest = 0;
static size_t total = 0;
static size_t avail = 0;

static lock_t lock = LOCK_CREATE();

static bool pmm_is_mem_avail(EFI_MEMORY_TYPE type)
{
    if (!boot_is_mem_ram(type))
    {
        return false;
    }

    switch (type)
    {
    case EfiConventionalMemory:
    case EfiLoaderCode:
    // EfiLoaderData is intentionally not included here as it's freed later in `init()`.
    case EfiBootServicesCode:
    case EfiBootServicesData:
        return true;
    default:
        return false;
    }
}

static inline void pmm_stack_push(pfn_t pfn)
{
    if (stack == NULL || location == 0)
    {
        page_stack_t* page = PFN_TO_VIRT(pfn);
        page->next = stack;
        stack = page;
        location = FREE_PAGE_MAX;

        return;
    }

    stack->pages[--location] = pfn;
}

static inline pfn_t pmm_stack_pop(void)
{
    if (location == FREE_PAGE_MAX)
    {
        if (stack == NULL)
        {
            return ERR;
        }

        page_stack_t* page = stack;
        stack = stack->next;
        location = (stack == NULL) ? FREE_PAGE_MAX : 0;
        return VIRT_TO_PFN(page);
    }

    return stack->pages[location++];
}

static inline pfn_t pmm_bitmap_set(size_t count, pfn_t maxPfn, pfn_t alignPfn)
{
    alignPfn = MAX(alignPfn, 1);
    maxPfn = MIN(maxPfn, CONFIG_PMM_BITMAP_MAX_ADDR / PAGE_SIZE);

    size_t index = bitmap_find_clear_region_and_set(&bitmap, 0, maxPfn, count, alignPfn);
    if (index == bitmap.length)
    {
        return ERR;
    }

    return (pfn_t)index;
}

static inline void pmm_bitmap_clear(pfn_t pfn, size_t pageAmount)
{
    bitmap_clear_range(&bitmap, pfn, pfn + pageAmount);
}

static void pmm_free_unlocked(pfn_t pfn)
{
    page_t* page = &pages[pfn];
    assert(page->ref > 0);
    if (--page->ref > 0)
    {
        return;
    }

    if (pfn >= CONFIG_PMM_BITMAP_MAX_ADDR / PAGE_SIZE)
    {
        pmm_stack_push(pfn);
    }
    else
    {
        pmm_bitmap_clear(pfn, 1);
    }

    avail++;
}

static void pmm_free_region_unlocked(pfn_t pfn, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        pmm_free_unlocked(pfn + i);
    }
}

static void pmm_detect_memory(const boot_memory_map_t* map)
{
    LOG_INFO("UEFI-provided memory map\n");

    for (size_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        if (boot_is_mem_ram(desc->Type))
        {
            total += desc->NumberOfPages;
        }

        pfn_t endPfn = VIRT_TO_PFN(desc->VirtualStart) + desc->NumberOfPages;
        highest = MAX(highest, endPfn);
    }

    LOG_INFO("page amount %llu\n", total);
}

static void pmm_init_refs(const boot_memory_map_t* map)
{
    size_t size = highest * sizeof(page_t);
    for (size_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        if (desc->Type == EfiConventionalMemory && desc->NumberOfPages >= BYTES_TO_PAGES(size))
        {
            pages = (page_t*)desc->VirtualStart;
            LOG_INFO("pages   [%p-%p]\n", pages, (uintptr_t)pages + size);
            return;
        }
    }

    panic(NULL, "Failed to allocate pmm refs array");
}

static void pmm_load_memory(const boot_memory_map_t* map)
{
    pfn_t pagesPfn = VIRT_TO_PFN(pages);

    for (size_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        pfn_t pfn = VIRT_TO_PFN(desc->VirtualStart);
        size_t amount = desc->NumberOfPages;

        if (pfn == pagesPfn)
        {
            // Skip the pages array.
            size_t toSkip = BYTES_TO_PAGES(highest * sizeof(page_t));
            pfn += toSkip;
            amount -= toSkip;
        }

        if (pmm_is_mem_avail(desc->Type))
        {
#ifndef NDEBUG
            // Clear the memory to deliberately cause corruption if the memory is actually being used.
            memset(PFN_TO_VIRT(pfn), 0xCC, amount * PAGE_SIZE);
#endif
            for (size_t j = 0; j < amount; j++)
            {
                page_t* page = &pages[pfn + j];
                page->ref = 1;
            }

            pmm_free_region_unlocked(pfn, amount);
        }
        else
        {
            for (size_t j = 0; j < amount; j++)
            {
                page_t* page = &pages[pfn + j];
                page->ref = UINT16_MAX;
            }

            LOG_INFO("reserve [%p-%p] pages=%d type=%s\n", PFN_TO_VIRT(pfn), PFN_TO_VIRT(pfn + amount), amount,
                efiMemTypeToString[desc->Type]);
        }
    }

    LOG_INFO("memory %llu MB (usable %llu MB reserved %llu MB)\n", (total * PAGE_SIZE) / 1000000,
        (pmm_avail_pages() * PAGE_SIZE) / 1000000, ((total - pmm_avail_pages()) * PAGE_SIZE) / 1000000);
}

void pmm_init(void)
{
    const boot_info_t* bootInfo = boot_info_get();
    const boot_memory_map_t* map = &bootInfo->memory.map;

    pmm_detect_memory(map);
    pmm_init_refs(map);
    pmm_load_memory(map);
}

pfn_t pmm_alloc(void)
{
    lock_acquire(&lock);
    pfn_t pfn = pmm_stack_pop();
    if (pfn == ERR)
    {
        pfn = pmm_bitmap_set(1, CONFIG_PMM_BITMAP_MAX_ADDR / PAGE_SIZE, 1);
    }

    if (pfn != ERR)
    {
        page_t* page = &pages[pfn];
        assert(page->ref == 0);
        page->ref = 1;
        avail--;
    }
    lock_release(&lock);

    if (pfn == ERR)
    {
        LOG_WARN("out of memory in pmm_alloc()\n");
    }

    return pfn;
}

uint64_t pmm_alloc_pages(pfn_t* pfns, size_t count)
{
    lock_acquire(&lock);
    for (size_t i = 0; i < count; i++)
    {
        pfns[i] = pmm_stack_pop();
        if (pfns[i] == ERR)
        {
            pfns[i] = pmm_bitmap_set(1, CONFIG_PMM_BITMAP_MAX_ADDR / PAGE_SIZE, 1);
        }

        if (pfns[i] == ERR)
        {
            LOG_WARN("out of memory in pmm_alloc_pages()\n");
            for (size_t j = 0; j < i; j++)
            {
                if (pfns[j] >= CONFIG_PMM_BITMAP_MAX_ADDR / PAGE_SIZE)
                {
                    pmm_stack_push(pfns[j]);
                }
                else
                {
                    pmm_bitmap_clear(pfns[j], 1);
                }
            }
            lock_release(&lock);
            return ERR;
        }
    }

    for (size_t i = 0; i < count; i++)
    {
        page_t* page = &pages[pfns[i]];
        assert(page->ref == 0);
        page->ref = 1;
    }

    avail -= count;
    lock_release(&lock);
    return 0;
}

pfn_t pmm_alloc_bitmap(size_t count, pfn_t maxPfn, pfn_t alignPfn)
{
    lock_acquire(&lock);
    pfn_t pfn = pmm_bitmap_set(count, maxPfn, alignPfn);
    if (pfn != ERR)
    {
        for (size_t i = 0; i < count; i++)
        {
            page_t* page = &pages[pfn + i];
            assert(page->ref == 0);
            page->ref = 1;
        }
        avail -= count;
    }
    lock_release(&lock);

    if (pfn == ERR)
    {
        LOG_WARN("out of memory in pmm_alloc_bitmap()\n");
    }

    return pfn;
}

void pmm_free(pfn_t pfn)
{
    lock_acquire(&lock);
    pmm_free_unlocked(pfn);
    lock_release(&lock);
}

void pmm_free_pages(pfn_t* pfns, size_t count)
{
    lock_acquire(&lock);
    for (size_t i = 0; i < count; i++)
    {
        pmm_free_unlocked(pfns[i]);
    }
    lock_release(&lock);
}

void pmm_free_region(pfn_t pfn, size_t count)
{
    lock_acquire(&lock);
    pmm_free_region_unlocked(pfn, count);
    lock_release(&lock);
}

uint64_t pmm_ref_inc(pfn_t pfn, size_t count)
{
    lock_acquire(&lock);
    for (size_t i = 0; i < count; i++)
    {
        page_t* page = &pages[pfn + i];
        if (page->ref == 0 || page->ref == UINT16_MAX)
        {
            for (size_t j = 0; j < i; j++)
            {
                pages[pfn + j].ref--;
            }
            lock_release(&lock);
            return ERR;
        }
        page->ref++;
    }
    uint64_t ret = pages[pfn].ref;
    lock_release(&lock);
    return ret;
}

size_t pmm_total_pages(void)
{
    lock_acquire(&lock);
    size_t ret = total;
    lock_release(&lock);
    return ret;
}

size_t pmm_avail_pages(void)
{
    lock_acquire(&lock);
    size_t ret = avail;
    lock_release(&lock);
    return ret;
}

size_t pmm_used_pages(void)
{
    lock_acquire(&lock);
    size_t ret = total - avail;
    lock_release(&lock);
    return ret;
}