#pragma once

#include "gop/gop.h"

#define VIRTUAL_MEMORY_LOAD_SPACE(addressSpace) asm volatile ("mov %0, %%cr3" : : "r" ((uint64_t)addressSpace))

#define PAGE_DIR_SET_FLAG(entry, flag) ((entry) |= (1UL << (flag)))
#define PAGE_DIR_CLEAR_FLAG(entry, flag) ((entry) &= ~(1UL << (flag)))
#define PAGE_DIR_GET_FLAG(entry, flag) (((entry) >> (flag)) & 1)

#define PAGE_DIR_GET_ADDRESS(entry) (((entry) & 0x000ffffffffff000) >> 12)
#define PAGE_DIR_SET_ADDRESS(entry, address) (entry = ((entry) & 0xfff0000000000fff) | (((address) & 0x000000ffffffffff) << 12))

enum PAGE_DIR_FLAGS 
{
    PAGE_DIR_PRESENT = 0,
    PAGE_DIR_READ_WRITE = 1,
    PAGE_DIR_USER_SUPERVISOR = 2,
    PAGE_DIR_WRITE_TROUGH = 3,
    PAGE_DIR_CACHE_DISABLED = 4,
    PAGE_DIR_ACCESSED = 5,
    PAGE_DIR_PAGE_SIZE = 7,
    PAGE_DIR_CUSTOM_0 = 9,
    PAGE_DIR_CUSTOM_1 = 10,
    PAGE_DIR_CUSTOM_2 = 11,
    PAGE_DIR_NX = 63
};

typedef uint64_t PageDirEntry;

typedef struct __attribute__((aligned(0x1000)))
{ 
    PageDirEntry Entries[512];
} PageDirectory;

typedef PageDirectory VirtualAddressSpace;

VirtualAddressSpace* virtual_memory_create();

void virtual_memory_remap_pages(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress, uint64_t pageAmount);

void virtual_memory_remap(VirtualAddressSpace* addressSpace, void* virtualAddress, void* physicalAddress);

void virtual_memory_erase(VirtualAddressSpace* addressSpace);

void virtual_memory_invalidate_page(void* address);