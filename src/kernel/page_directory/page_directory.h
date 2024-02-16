#pragma once

#include "gop/gop.h"
#include "memory/memory.h"

//TODO: Redo this, add better flag handling, add global pages etc.

#define USER_ADDRESS_SPACE_TOP 0x100000000
#define USER_ADDRESS_SPACE_BOTTOM 0

#define PAGE_DIR_PRESENT (1 << 0)
#define PAGE_DIR_READ_WRITE (1 << 1)
#define PAGE_DIR_USER_SUPERVISOR (1 << 2)
#define PAGE_DIR_WRITE_TROUGH (1 << 3)
#define PAGE_DIR_CACHE_DISABLED (1 << 4)
#define PAGE_DIR_ACCESSED (1 << 5)
#define PAGE_DIR_PAGE_SIZE (1 << 7)
#define PAGE_DIR_CUSTOM_0 (1 << 9)
#define PAGE_DIR_CUSTOM_1 (1 << 10)
#define PAGE_DIR_CUSTOM_2 (1 << 11)

#define PAGE_DIR_GET_FLAG(entry, flag) (((entry) >> (flag)) & 1)
#define PAGE_DIR_GET_ADDRESS(entry) ((entry) & 0x000ffffffffff000)

#define PAGE_DIR_ENTRY_CREATE(address, flags) ((((uint64_t)address >> 12) & 0x000000ffffffffff) << 12) | ((uint64_t)flags) | ((uint64_t)PAGE_DIR_PRESENT)

#define PAGE_DIRECTORY_LOAD_SPACE(pageDirectory) asm volatile ("movq %0, %%cr3" : : "r" ((uint64_t)pageDirectory))

typedef uint64_t PageDirectoryEntry;

typedef struct __attribute__((aligned(0x1000)))
{ 
    PageDirectoryEntry entries[512];
} PageDirectory;

extern PageDirectory* kernelPageDirectory;

extern void page_directory_invalidate_page(void* virtualAddress);

void page_directory_init(EfiMemoryMap* memoryMap, Framebuffer* screenbuffer);

PageDirectory* page_directory_new();

void page_directory_remap_pages(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags);

void page_directory_remap(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint16_t flags);

void* page_directory_get_physical_address(PageDirectory const* pageDirectory, void* virtualAddress);

void page_directory_free(PageDirectory* pageDirectory);