#pragma once

#include <efi.h>
#include <efilib.h>

#define PAGE_DIRECTORY_LOAD(pageDirectory) asm volatile ("mov %0, %%cr3" : : "r" ((uint64_t)pageDirectory))

#define PAGE_DIRECTORY_ENTRY_CREATE(address, flags) ((((uint64_t)address >> 12) & 0x000000ffffffffff) << 12) | ((uint64_t)flags) | ((uint64_t)PAGE_FLAG_PRESENT)

#define PAGE_DIRECTORY_GET_FLAG(entry, flag) (((entry) >> (flag)) & 1)
#define PAGE_DIRECTORY_GET_ADDRESS(entry) ((entry) & 0x000ffffffffff000)

#define PAGE_FLAG_PRESENT (1 << 0)
#define PAGE_FLAG_WRITE (1 << 1)
#define PAGE_FLAG_USER_SUPERVISOR (1 << 2)
#define PAGE_FLAG_WRITE_TROUGH (1 << 3)
#define PAGE_FLAG_CACHE_DISABLED (1 << 4)
#define PAGE_FLAG_ACCESSED (1 << 5)
#define PAGE_FLAG_PAGE_SIZE (1 << 7)
#define PAGE_DIR_CUSTOM_0 (1 << 9)
#define PAGE_DIR_CUSTOM_1 (1 << 10)
#define PAGE_DIR_CUSTOM_2 (1 << 11)

typedef uint64_t PageDirectoryEntry;

typedef struct __attribute__((aligned(0x1000)))
{ 
    PageDirectoryEntry entries[512];
} PageDirectory;

PageDirectory* page_directory_new();

void page_directory_map_pages(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint64_t pageAmount, uint16_t flags);

void page_directory_map(PageDirectory* pageDirectory, void* virtualAddress, void* physicalAddress, uint16_t flags);