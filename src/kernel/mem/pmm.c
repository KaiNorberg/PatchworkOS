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

static pmm_ref_t* refs = NULL;

static page_stack_t* stack = NULL;
static size_t location = FREE_PAGE_MAX;

BITMAP_CREATE_ONE(bitmap, CONFIG_PMM_BITMAP_MAX_ADDR / PAGE_SIZE);

static uintptr_t highest = 0;
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

static inline pmm_ref_t* pmm_ref_get(void* address)
{
    return &refs[PML_HIGHER_TO_LOWER(address) / PAGE_SIZE];
}

static inline void pmm_ref_set(void* address, size_t count, pmm_ref_t value)
{
    pmm_ref_t* ref = pmm_ref_get(address);
    for (size_t i = 0; i < count; i++)
    {
        ref[i] = value;
    }
}

static inline size_t pmm_refs_size(void)
{
    return (PML_HIGHER_TO_LOWER(highest) / PAGE_SIZE) * sizeof(pmm_ref_t);
}

static inline void pmm_stack_push(void* addr)
{
    if (stack == NULL || location == 0)
    {
        page_stack_t* page = addr;
        page->next = stack;
        stack = page;
        location = FREE_PAGE_MAX;

        return;
    }

    stack->pages[--location] = addr;
}

static inline void* pmm_stack_pop(void)
{
    if (location == FREE_PAGE_MAX)
    {
        if (stack == NULL)
        {
            return NULL;
        }

        void* addr = stack;
        stack = stack->next;
        location = (stack == NULL) ? FREE_PAGE_MAX : 0;
        return addr;
    }

    return stack->pages[location++];
}

static inline void* pmm_bitmap_set(size_t count, uintptr_t maxAddr, size_t alignment)
{
    alignment = MAX(ROUND_UP(alignment, PAGE_SIZE), PAGE_SIZE);
    maxAddr = MIN(maxAddr, CONFIG_PMM_BITMAP_MAX_ADDR);

    size_t index = bitmap_find_clear_region_and_set(&bitmap, 0, maxAddr / PAGE_SIZE, count, alignment / PAGE_SIZE);
    if (index == bitmap.length)
    {
        return NULL;
    }

    return (void*)PML_LOWER_TO_HIGHER(index * PAGE_SIZE);
}

static inline void pmm_bitmap_clear(void* addr, size_t pageAmount)
{
    addr = (void*)ROUND_DOWN(addr, PAGE_SIZE);

    size_t index = PML_HIGHER_TO_LOWER(addr) / PAGE_SIZE;
    bitmap_clear_range(&bitmap, index, pageAmount);
}

static void pmm_free_unlocked(void* address)
{
    pmm_ref_t* ref = pmm_ref_get(address);
    assert(*ref > 0);
    (*ref)--;
    if (*ref > 0)
    {
        return;
    }

    if (address >= (void*)PML_LOWER_TO_HIGHER(CONFIG_PMM_BITMAP_MAX_ADDR))
    {
        pmm_stack_push(address);
    }
    else if (address >= (void*)PML_LOWER_TO_HIGHER(0))
    {
        pmm_bitmap_clear(address, 1);
    }
    else
    {
        panic(NULL, "Attempt to free lower half address %p", address);
    }

    avail++;
}

static void pmm_free_region_unlocked(void* address, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        pmm_free_unlocked((void*)((uintptr_t)address + (i * PAGE_SIZE)));
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
        highest = MAX(highest, (uintptr_t)desc->VirtualStart + (desc->NumberOfPages * PAGE_SIZE));
    }

    LOG_INFO("page amount %llu\n", total);
    LOG_INFO("highest address %p\n", highest);
}

static void pmm_init_refs(const boot_memory_map_t* map)
{
    size_t size = pmm_refs_size();
    size_t pages = BYTES_TO_PAGES(size);
    for (size_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);
        if (desc->Type == EfiConventionalMemory && desc->NumberOfPages >= pages)
        {
            refs = (pmm_ref_t*)desc->VirtualStart;
            memset(refs, -1, pages * PAGE_SIZE);
            LOG_INFO("pmm ref [%p-%p]\n", refs, (uintptr_t)refs + pages * PAGE_SIZE);
            return;
        }
    }

    panic(NULL, "Failed to allocate pmm refs array");
}

static void pmm_load_memory(const boot_memory_map_t* map)
{
    for (size_t i = 0; i < map->length; i++)
    {
        const EFI_MEMORY_DESCRIPTOR* desc = BOOT_MEMORY_MAP_GET_DESCRIPTOR(map, i);

        uintptr_t address = desc->VirtualStart;
        size_t pages = desc->NumberOfPages;
        if (address == (uintptr_t)refs)
        {
            // Skip the refs array.
            size_t refPages = BYTES_TO_PAGES(pmm_refs_size());
            assert(pages >= refPages);
            address += refPages * PAGE_SIZE;
            pages -= refPages;
        }

        if (pmm_is_mem_avail(desc->Type))
        {
#ifndef NDEBUG
            // Clear the memory to deliberately cause corruption if the memory is actually being used.
            memset((void*)address, 0xCC, pages * PAGE_SIZE);
#endif
            pmm_ref_set((void*)address, pages, 1);
            pmm_free_region_unlocked((void*)address, pages);
        }
        else
        {
            LOG_INFO("reserve [%p-%p] pages=%d type=%s\n", address, (uintptr_t)address + (pages * PAGE_SIZE), pages,
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

void* pmm_alloc(void)
{
    lock_acquire(&lock);
    void* addr = pmm_stack_pop();
    if (addr == NULL)
    {
        addr = pmm_bitmap_set(1, CONFIG_PMM_BITMAP_MAX_ADDR, PAGE_SIZE);
    }

    if (addr != NULL)
    {
        pmm_ref_t* ref = pmm_ref_get(addr);
        assert(*ref == 0);
        *ref = 1;
        avail--;
    }
    lock_release(&lock);

    if (addr == NULL)
    {
        LOG_WARN("out of memory in pmm_alloc()\n");
    }

    return addr;
}

uint64_t pmm_alloc_pages(void** addresses, size_t count)
{
    lock_acquire(&lock);
    for (size_t i = 0; i < count; i++)
    {
        addresses[i] = pmm_stack_pop();
        if (addresses[i] == NULL)
        {
            addresses[i] = pmm_bitmap_set(1, CONFIG_PMM_BITMAP_MAX_ADDR, PAGE_SIZE);
        }

        if (addresses[i] == NULL)
        {
            LOG_WARN("out of memory in pmm_alloc_pages()\n");
            for (size_t j = 0; j < i; j++)
            {
                if (addresses[j] >= (void*)PML_LOWER_TO_HIGHER(CONFIG_PMM_BITMAP_MAX_ADDR))
                {
                    pmm_stack_push(addresses[j]);
                }
                else
                {
                    pmm_bitmap_clear(addresses[j], 1);
                }
            }
            lock_release(&lock);
            return ERR;
        }
    }

    for (size_t i = 0; i < count; i++)
    {
        pmm_ref_t* ref = pmm_ref_get(addresses[i]);
        assert(*ref == 0);
        *ref = 1;
    }

    avail -= count;
    lock_release(&lock);
    return 0;
}

void* pmm_alloc_bitmap(size_t count, uintptr_t maxAddr, uint64_t alignment)
{
    lock_acquire(&lock);
    void* addr = pmm_bitmap_set(count, maxAddr, alignment);
    if (addr != NULL)
    {
        for (size_t i = 0; i < count; i++)
        {
            pmm_ref_t* ref = pmm_ref_get((void*)((uintptr_t)addr + (i * PAGE_SIZE)));
            assert(*ref == 0);
            *ref = 1;
        }
        avail -= count;
    }
    lock_release(&lock);

    if (addr == NULL)
    {
        LOG_WARN("out of memory in pmm_alloc_bitmap()\n");
    }

    return addr;
}

void pmm_free(void* address)
{
    lock_acquire(&lock);
    pmm_free_unlocked(address);
    lock_release(&lock);
}

void pmm_free_pages(void** addresses, size_t count)
{
    lock_acquire(&lock);
    for (size_t i = 0; i < count; i++)
    {
        pmm_free_unlocked(addresses[i]);
    }
    lock_release(&lock);
}

void pmm_free_region(void* address, size_t count)
{
    lock_acquire(&lock);
    pmm_free_region_unlocked(address, count);
    lock_release(&lock);
}

uint64_t pmm_ref_inc(void* address)
{
    lock_acquire(&lock);
    pmm_ref_t* ref = pmm_ref_get(address);
    assert(*ref != PAGE_REF_MAX);
    (*ref)++;
    uint64_t ret = *ref;
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