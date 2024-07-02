#pragma once

#include "defs.h"

// Note: Page table does not perform error checking.

#define PAGE_PRESENT (1 << 0)
#define PAGE_WRITE (1 << 1)
#define PAGE_USER (1 << 2)
#define PAGE_WRITE_TROUGH (1 << 3)
#define PAGE_CACHE_DISABLED (1 << 4)
#define PAGE_ACCESSED (1 << 5)
#define PAGE_DIRTY (1 << 6)
#define PAGE_PAGE_SIZE (1 << 7)
#define PAGE_GLOBAL (1 << 8)

// If the page is owned by the page table and should be freed when the page is unmapped.
#define PAGE_OWNED (1 << 9)

#define PAGE_ENTRY_AMOUNT 512
#define PAGE_ENTRY_GET_ADDRESS(entry) VMM_LOWER_TO_HIGHER((entry) & 0x000FFFFFFFFFF000)

#define PAGE_INVALIDATE(address) asm volatile("invlpg %0" : : "m"(address))

/*#define PML_GET_INDEX(address, level) \
    (((uint64_t)(address) & ((uint64_t)0x1FF << (((level) - 1) * 9 + 12))) >> (((level) - 1) * 9 + 12))*/
#define PML_GET_INDEX(address, level) (((uint64_t)(address) >> (((level) - 1) * 9 + 12)) & 0x1FF)

typedef uint64_t pml_entry_t;

typedef struct
{
    pml_entry_t entries[PAGE_ENTRY_AMOUNT];
} pml_t;

pml_t* pml_new(void);

void pml_free(pml_t* table);

void pml_load(pml_t* table);

void* pml_phys_addr(pml_t* table, const void* virtAddr);

bool pml_mapped(pml_t* table, const void* virtAddr, uint64_t pageAmount);

void pml_map(pml_t* table, void* virtAddr, void* physAddr, uint64_t pageAmount, uint64_t flags);

void pml_unmap(pml_t* table, void* virtAddr, uint64_t pageAmount);

void pml_change_flags(pml_t* table, void* virtAddr, uint64_t pageAmount, uint64_t flags);
