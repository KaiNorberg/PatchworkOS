#pragma once

#include <stdint.h>

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

#define PAGE_TABLE_GET_FLAG(entry, flag) (((entry) >> (flag)) & 1)

#define PAGE_TABLE_GET_ADDRESS(entry) ((entry) & 0x000ffffffffff000)

#define PAGE_TABLE_LOAD(pageTable) asm volatile("mov %0, %%cr3" : : "r"((uint64_t)pageTable))

typedef uint64_t PageEntry;

typedef struct
{
    PageEntry entries[512];
} PageTable;

PageTable* page_table_new(void);

void page_table_map_pages(PageTable* pageTable, void* virtAddr, void* physAddr, uint64_t pageAmount, uint16_t flags);

void page_table_map(PageTable* pageTable, void* virtAddr, void* physAddr, uint16_t flags);